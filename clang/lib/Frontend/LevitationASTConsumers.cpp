//==-- LevitationASTConsumers.cpp -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//
// Levitation AST Consumers Implementations.
//
//===----------------------------------------------------------------------===//

#include "clang/Frontend/LevitationASTConsumers.h"
#include "clang/AST/AST.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticFrontend.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Bitcode/BitstreamWriter.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;

//===----------------------------------------------------------------------===//
/// ASTPrinter - Pretty-printer and dumper of ASTs

namespace {

  class ValidatedDependenciesMap : public Sema::LevitationDepenciesMap {
    bool HasMissingDependencies = false;
  public:
    void setHasMissingDependencies() { HasMissingDependencies = true; }
    bool hasMissingDependencies() { return HasMissingDependencies; }
  };

  struct ValidatedDependencies {
    ValidatedDependenciesMap DeclarationDependencies;
    ValidatedDependenciesMap DefinitinoDependencies;
    bool hasMissingDependencies() {
      return DeclarationDependencies.hasMissingDependencies() ||
             DefinitinoDependencies.hasMissingDependencies();
    }
  };

  inline const DiagnosticBuilder &operator<<(
      const DiagnosticBuilder &DB,
      const Sema::LevitationPackageDependency &V
  ) {
    std::string str;
    llvm::raw_string_ostream strm(str);
    V.print(strm);
    DB.AddString(str);
    return DB;
  }


  class DependenciesValidator {
      StringRef SourcesRoot;
      StringRef FileExtension;
      FileManager *FileMgr;
      DiagnosticsEngine &Diag;
  public:
    DependenciesValidator(
      StringRef sourcesRoot,
      StringRef fileExtension,
      FileManager *fileMgr,
      DiagnosticsEngine &diag
    ) : SourcesRoot(sourcesRoot),
        FileExtension(fileExtension),
        FileMgr(fileMgr),
        Diag(diag)
    {}

    ValidatedDependenciesMap validate(const Sema::LevitationDepenciesMap& Dependencies) {
      ValidatedDependenciesMap ValidatedDependencies;
      for (const auto &Dep : Dependencies) {
        validate(ValidatedDependencies, Dep.second);
      }
      return ValidatedDependencies;
    }
  private:

    void validate(
            ValidatedDependenciesMap &Map,
            const Sema::LevitationPackageDependency &Dep
    ) {
      Sema::DependencyPath Path;

      Path.append(SourcesRoot.begin(), SourcesRoot.end());

      Sema::DependencyComponentsVector ValidatedComponents;

      auto UnvalidatedComponents = Dep.getComponents();

      bool validated = buildPath(Path, ValidatedComponents, UnvalidatedComponents);

      if (!validated) {
        diagMissingDependency(Dep);
        Map.setHasMissingDependencies();
        return;
      }

      Sema::LevitationPackageDependency Validated(std::move(ValidatedComponents));
      Validated.addUses(Dep.getUses());
      Validated.setPath(std::move(Path));

      Map.mergeDependency(std::move(Validated));
    }

    bool buildPath(
        Sema::DependencyPath &Path,
        Sema::DependencyComponentsVector &ValidatedComponents,
        Sema::DependencyComponentsArrRef &UnvalidatedComponents
    ) {
      bool Valid = false;
      for (size_t i = 0, e = UnvalidatedComponents.size(); i != e; ++i) {

        const StringRef &Component = UnvalidatedComponents[i];
        ValidatedComponents.push_back(Component);

        llvm::sys::path::append(Path, Component);

        if (isDirectory(Path))
          continue;

        Path.append(FileExtension.begin(), FileExtension.end());

        if (isFile(Path))
          Valid = true;

        break;
      }
      return Valid;
    }

    bool isDirectory(const SmallVectorImpl<char> &Dir) {
      return (bool)FileMgr->getDirectory(StringRef(Dir.begin(), Dir.size()));
    }

    bool isFile(const SmallVectorImpl<char> &FName) {
      return (bool)FileMgr->getFile(StringRef(FName.begin(), FName.size()));
    }

    void diagMissingDependency(const Sema::LevitationPackageDependency &Dep) {
      Diag.Report(diag::err_levitation_dependency_missed)
      << Dep << Dep.getFirstUse().Location;
    }
  };

  class DependenciesSerializer {
  public:
      virtual ~DependenciesSerializer() = default;
      virtual void serialize(const ValidatedDependencies& Deps) {}
  };

  class DependenciesBitstreamSerializer : public DependenciesSerializer {
      // TODO Levitation: may be in future introduce LevitationMetadataWriter.
      llvm::raw_ostream &OutputStream;
  public:
      DependenciesBitstreamSerializer(llvm::raw_ostream &OS)
      : OutputStream(OS) {}

      void serialize(const ValidatedDependencies &Deps) override {
        // TODO Levitation
        DependenciesSerializer::serialize(Deps);
      }
  };

  class DependenciesFile {
  public:
    enum StatusEnum {
      Good,
      HasStreamErrors,
      FiledToRename
    };

  private:
    StringRef TargetFileName;
    StringRef TempPath;
    std::unique_ptr<llvm::raw_fd_ostream> OutputStream;
    StatusEnum Status;
  public:
    DependenciesFile(StringRef targetFileName)
      : TargetFileName(targetFileName), Status(Good) {}

    // TODO Levitation: we could introduce some generic template,
    //   something like scope_exit, but with ability to convert it to bool.
    class FileScope {
        DependenciesFile *File;
    public:
        FileScope(DependenciesFile *F) : File(F) {}
        FileScope(FileScope &&dying) : File(dying.File) { dying.File = nullptr; }
        ~FileScope() { if (File) File->close(); }

        operator bool() const { return File; }
        llvm::raw_ostream& getOutputStream() { return *File->OutputStream; }
    };

    FileScope open() {
      // Write to a temporary file and later rename it to the actual file, to avoid
      // possible race conditions.
      SmallString<128> TempPath;
      TempPath = TargetFileName;
      TempPath += "-%%%%%%%%";
      int fd;
      if (llvm::sys::fs::createUniqueFile(TempPath, fd, TempPath))
        return FileScope(nullptr);

      OutputStream.reset(new llvm::raw_fd_ostream(fd, /*shouldClose=*/true));

      return FileScope(this);
    }

    void close() {
      OutputStream->close();
      if (OutputStream->has_error()) {
        OutputStream->clear_error();
        Status = HasStreamErrors;
      }

      if (llvm::sys::fs::rename(TempPath, TargetFileName)) {
        llvm::sys::fs::remove(TempPath);
        Status = FiledToRename;
      }
    }

    StatusEnum getStatus() const {
      return Status;
    }
  };

  class ASTDependenciesProcessor : public SemaConsumer {
    Sema *SemaObj;
    CompilerInstance &CI;
  public:
    ASTDependenciesProcessor(CompilerInstance &ci)
      : CI(ci) {}

    void HandleTranslationUnit(ASTContext &Context) override {

      ValidatedDependenciesMap ValidatedDeclarationDependencies;
      ValidatedDependenciesMap ValidatedDefinitionDependencies;

      DependenciesValidator Validator(
          CI.getFrontendOpts().LevitationSourcesRootDir,
          CI.getFrontendOpts().LevitationSourceFileExtension,
          &CI.getFileManager(),
          CI.getDiagnostics()
      );

      ValidatedDependencies Dependencies {
        Validator.validate(SemaObj->getLevitationDeclarationDependencies()),
        Validator.validate(SemaObj->getLevitationDefinitionDependencies())
      };

      if (Dependencies.hasMissingDependencies())
        return;

      auto F = createDependenciesFile();
      if (auto OpenScope = F.open()) {
        DependenciesBitstreamSerializer Serializer(OpenScope.getOutputStream());
        Serializer.serialize(Dependencies);
      }
      if (F.getStatus() != DependenciesFile::Good) {
        diagDependencyFileIOIssues(F.getStatus());
      }
    }

    void InitializeSema(Sema &S) override {
      SemaObj = &S;
    }

    void ForgetSema() override {
      SemaObj = nullptr;
    }
  private:
    DependenciesFile createDependenciesFile() {
      return DependenciesFile(CI.getFrontendOpts().LevitationDependenciesOutputFile);
    }

    void diagDependencyFileIOIssues(DependenciesFile::StatusEnum status) {
      auto &Diag = CI.getDiagnostics();
      switch (status) {
        case DependenciesFile::HasStreamErrors:
          Diag.Report(diag::err_fe_levitation_dependency_file_io_troubles);
          break;
        case DependenciesFile::FiledToRename:
          Diag.Report(diag::err_fe_levitation_dependency_file_failed_to_create);
          break;
      }
    }
  };

} // end anonymous namespace

namespace clang {

std::unique_ptr<ASTConsumer> CreateDependenciesASTProcessor(CompilerInstance &CI) {
  return llvm::make_unique<ASTDependenciesProcessor>(CI);
}

}

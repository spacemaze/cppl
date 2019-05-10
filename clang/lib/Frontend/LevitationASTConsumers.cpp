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
#include "clang/Levitation/Dependencies.h"
#include "clang/Levitation/File.h"
#include "clang/Levitation/Serialization.h"
#include "llvm/Bitcode/BitstreamWriter.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::levitation;

//===----------------------------------------------------------------------===//
/// ASTPrinter - Pretty-printer and dumper of ASTs

namespace {

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

    ValidatedDependenciesMap validate(const DepenciesMap& Dependencies) {
      ValidatedDependenciesMap ValidatedDependencies;
      for (const auto &Dep : Dependencies) {
        validate(ValidatedDependencies, Dep.second);
      }
      return ValidatedDependencies;
    }
  private:

    void validate(
            ValidatedDependenciesMap &Map,
            const PackageDependency &Dep
    ) {
      DependencyPath Path;

      Path.append(SourcesRoot.begin(), SourcesRoot.end());

      DependencyComponentsVector ValidatedComponents;

      auto UnvalidatedComponents = Dep.getComponents();

      bool validated = buildPath(Path, ValidatedComponents, UnvalidatedComponents);

      if (!validated) {
        diagMissingDependency(Dep);
        Map.setHasMissingDependencies();
        return;
      }

      PackageDependency Validated(std::move(ValidatedComponents));
      Validated.addUses(Dep.getUses());
      Validated.setPath(std::move(Path));

      Map.mergeDependency(std::move(Validated));
    }

    bool buildPath(
        DependencyPath &Path,
        DependencyComponentsVector &ValidatedComponents,
        DependencyComponentsArrRef &UnvalidatedComponents
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

    void diagMissingDependency(const levitation::PackageDependency &Dep) {
      // FIXME Levitation: SourceRange is not printed correctly.
      Diag.Report(
          Dep.getFirstUse().Location.getBegin(),
          diag::err_levitation_dependency_missed
      ) << Dep << Dep.getFirstUse().Location;
    }
  };

  class ASTDependenciesProcessor : public SemaConsumer {
    Sema *SemaObj;
    CompilerInstance &CI;
    std::string CurrentInputFile;
  public:
    ASTDependenciesProcessor(CompilerInstance &ci, StringRef currentInputFile)
      : CI(ci), CurrentInputFile(currentInputFile) {}

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
        Validator.validate(SemaObj->getLevitationDefinitionDependencies()),
        CurrentInputFile
      };

      if (Dependencies.hasMissingDependencies())
        return;

      auto F = createFile();

      if (auto OpenedFile = F.open()) {
        auto Writer = CreateBitstreamWriter(OpenedFile.getOutputStream());
        Writer->writeAndFinalize(Dependencies);
      }

      if (F.hasErrors()) {
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
    File createFile() {
      return File(CI.getFrontendOpts().LevitationDependenciesOutputFile);
    }

    void diagDependencyFileIOIssues(File::StatusEnum status) {
      auto &Diag = CI.getDiagnostics();
      switch (status) {
        case File::HasStreamErrors:
          Diag.Report(diag::err_fe_levitation_dependency_file_io_troubles);
          break;
        case File::FiledToRename:
          Diag.Report(diag::err_fe_levitation_dependency_file_failed_to_create);
          break;
        default:
          break;
      }
    }
  };

} // end anonymous namespace

namespace clang {

std::unique_ptr<ASTConsumer> CreateDependenciesASTProcessor(
    CompilerInstance &CI,
    StringRef InFile
) {
  return llvm::make_unique<ASTDependenciesProcessor>(CI, InFile);
}

}

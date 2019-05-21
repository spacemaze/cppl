//==-- LevitationFrontendActions.cpp --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Frontend/LevitationFrontendActions.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTDiagnostic.h"
#include "clang/AST/ASTImporter.h"
#include "clang/AST/ASTImporterLookupTable.h"
#include "clang/Basic/FileManager.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Frontend/LevitationASTConsumers.h"
#include "clang/Frontend/MultiplexConsumer.h"
#include "clang/Frontend/Utils.h"
#include "clang/Levitation/FileExtensions.h"
#include "clang/Levitation/WithOperator.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Sema/TemplateInstCallback.h"
#include "clang/Serialization/ASTBitCodes.h"
#include "clang/Serialization/ASTReader.h"
#include "clang/Serialization/ASTWriter.h"
#include "clang/Serialization/GlobalModuleIndex.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/SaveAndRestore.h"
#include "llvm/Support/YAMLTraits.h"
#include <memory>
#include <system_error>
#include <utility>

using namespace clang;

namespace {

typedef std::vector<std::unique_ptr<ASTConsumer>> ConsumersVector;

struct OutputFileHandle {
  std::unique_ptr<llvm::raw_pwrite_stream> OutputStream;
  std::string Path;

  static OutputFileHandle makeInvalid() {
    return {nullptr, ""};
  }

  bool isInvalid() const { return !(bool)OutputStream; };
};

// Cloned with some alterations from GeneratePCHAction.
OutputFileHandle CreateOutputFile(CompilerInstance &CI, StringRef InFile) {
  // Comment from original method:
  // We use createOutputFile here because this is exposed via libclang, and we
  // must disable the RemoveFileOnSignal behavior.
  // We use a temporary to avoid race conditions.

  StringRef Extension = "";
  SmallString<256> OutputPath(CI.getFrontendOpts().OutputFile);

  std::string OutputPathName, TempPathName;
  std::error_code EC;

  std::unique_ptr<raw_pwrite_stream> OS = CI.createOutputFile(
      OutputPath,
      EC,
      true,
      false,
      InFile,
      Extension,
      true,
      false,
      &OutputPathName,
      &TempPathName
  );

  if (!OS) {
    CI.getDiagnostics().Report(diag::err_fe_unable_to_open_output)
    << OutputPath
    << EC.message();
    return OutputFileHandle::makeInvalid();
  }

  CI.addOutputFile({
      (OutputPathName != "-") ? OutputPathName : "",
      TempPathName
  });

  return { std::move(OS), std::move(OutputPathName) };
}


// Cloned with some alterations from GeneratePCHAction::CreateASTConsumer.
// We don't consider RelocatablePCH as an option for Build AST stage.
static std::unique_ptr<ASTConsumer>
CreateGeneratePCHConsumer(
    CompilerInstance &CI,
    StringRef InFile
) {
  OutputFileHandle OutputFile = CreateOutputFile(CI, InFile);

  if (OutputFile.isInvalid())
    return nullptr;

  const auto &FrontendOpts = CI.getFrontendOpts();
  auto Buffer = std::make_shared<PCHBuffer>();
  std::vector<std::unique_ptr<ASTConsumer>> Consumers;

  Consumers.push_back(llvm::make_unique<PCHGenerator>(
      CI.getPreprocessor(),
      CI.getModuleCache(),
      OutputFile.Path,
      "",
      Buffer,
      FrontendOpts.ModuleFileExtensions,
      CI.getPreprocessorOpts().AllowPCHWithCompilerErrors,
      FrontendOpts.IncludeTimestamps,
      +CI.getLangOpts().CacheGeneratedPCH
  ));

  Consumers.push_back(CI.getPCHContainerWriter().CreatePCHContainerGenerator(
      CI,
      InFile,
      OutputFile.Path,
      std::move(OutputFile.OutputStream),
      Buffer
  ));

  return llvm::make_unique<MultiplexConsumer>(std::move(Consumers));
}

class MultiplexConsumerBuilder {

  bool Successful = true;
  std::vector<std::unique_ptr<ASTConsumer>> Consumers;

public:

  using BuilderTy = MultiplexConsumerBuilder;

  BuilderTy &addRequired(std::unique_ptr<ASTConsumer> &&Consumer) {
    if (Successful && Consumer)
      Consumers.push_back(std::move(Consumer));
    else
      Successful = false;
    return *this;
  }

  BuilderTy &addNotNull(std::unique_ptr<ASTConsumer> &&Consumer) {
    assert(Consumer && "Consumer must be non-null pointer");
    return addRequired(std::move(Consumer));
  }

  std::unique_ptr<MultiplexConsumer> done() {
    return Successful ?
        llvm::make_unique<MultiplexConsumer>(std::move(Consumers)) :
        nullptr;
  }
};

} // end of anonymous namespace

namespace clang {

class LevitationModulesReader : public ASTReader {
  StringRef MainFile;
  DiagnosticsEngine &Diags;
public:

  LevitationModulesReader(
      CompilerInstance &CompilerInst,
      StringRef mainFile
  ) :
    ASTReader(
      CompilerInst.getPreprocessor(),
      CompilerInst.getModuleCache(),
      &CompilerInst.getASTContext(),
      CompilerInst.getPCHContainerReader(),
      {}
    ),
    MainFile(mainFile),
    Diags(CompilerInst.getDiagnostics())
  {}

  using OpenedScope = levitation::ScopeExit<std::function<void()>>;

  OpenedScope open() {
    auto Context = beginRead();
    return OpenedScope([this, Context] {
        close(Context);
    });
  }

  // close() method is private, see below

  bool hasErrors() const {
    return ReadResult != Success;
  }

  ASTReadResult getStatus() const {
    return ReadResult;
  }

  using OnFailFn = std::function<void(
      DiagnosticsEngine &,
      StringRef,
      ASTReader::ASTReadResult
  )>;

  void readDependency(
      StringRef Dependency,
      OnFailFn &&OnFail
  ) {

    auto Res = read(
        Dependency,
        serialization::MK_LevitationDependency,
        /*ReadDeclarationsOnly=*/true
    );

    if (Res != ASTReader::Success)
      OnFail(Diags, Dependency, Res);
  }

private:

  unsigned NumModules = 0;
  unsigned PreviousGeneration;
  SmallVector<ImportedModule, 16> Loaded;
  ASTReadResult ReadResult;
  serialization::ModuleKind LastReadModuleKind = serialization::MK_MainFile;

  void close(const OpenedReaderContext &OpenedContext) {
    return endRead(OpenedContext);
  }

  OpenedReaderContext beginRead() {
    return BeginRead(
        PreviousGeneration,
        NumModules,
        SourceLocation(),
        ARR_None
    );
  }

  void endRead(const OpenedReaderContext &OpenedContext) {

    // Read main file after all dependencies.
    if (MainFile.size()) {
      ReadResult = read(
          MainFile,
          serialization::MK_MainFile,
          /*ReadDeclarationsOnly =*/false
      );
    }

    if (ReadResult == Success) {
      ReadResult = EndRead(
          OpenedContext,
          std::move(Loaded),
          LastReadModuleKind,
          SourceLocation(),
          ARR_None,
          PreviousGeneration,
          NumModules
      );
    }
  }

  ASTReadResult read(
      StringRef FileName,
      serialization::ModuleKind Type,
      bool DeclarationsOnly
  ) {
    LastReadModuleKind = Type;

    // FIXME Levitation: should pass as additional parameter?
    llvm::SaveAndRestore<bool>
    ReadDeclarationsOnlyFlagScope(ReadDeclarationsOnly, DeclarationsOnly);

    switch (ASTReadResult ReadResult = ReadASTCore(
        FileName,
        Type,
        SourceLocation(),
        /*ImportedBy=*/nullptr,
        Loaded,
        /*ExpectedSize=*/0,
        /*ExpectedModTime=*/0,
        ASTFileSignature(),
        ARR_None
    )) {
    case Failure:
    case Missing:
    case OutOfDate:
    case VersionMismatch:
    case ConfigurationMismatch:
    case HadErrors: {
      llvm::SmallPtrSet<ModuleFile *, 4> LoadedSet;
      for (const ImportedModule &IM : Loaded)
        LoadedSet.insert(IM.Mod);

      ModuleMgr.removeModules(ModuleMgr.begin() + NumModules, LoadedSet,
                              PP.getLangOpts().Modules
                                  ? &PP.getHeaderSearchInfo().getModuleMap()
                                  : nullptr);

      // If we find that any modules are unusable, the global index is going
      // to be out-of-date. Just remove it.
      GlobalIndex.reset();
      ModuleMgr.setGlobalIndex(nullptr);
      return ReadResult;
    }
    case Success:
      return Success;
    }
  }
};

} // end of namespace clang

std::unique_ptr<ASTConsumer> LevitationBuildASTAction::CreateASTConsumer(
    clang::CompilerInstance &CI,
    llvm::StringRef InFile
) {

  return MultiplexConsumerBuilder()
      .addNotNull(CreateParserPostProcessor())
      .addNotNull(CreateDependenciesASTProcessor(CI, InFile))
      .addRequired(CreateGeneratePCHConsumer(CI, InFile))
  .done();
}

void LevitationBuildObjectAction::ExecuteAction() {
  // ASTFrontendAction is used as source

  CompilerInstance &CI = getCompilerInstance();
  if (!CI.hasPreprocessor())
    llvm_unreachable("Only actions with preprocessor are supported.");

  // FIXME: Move the truncation aspect of this into Sema, we delayed this till
  // here so the source manager would be initialized.
  if (hasCodeCompletionSupport() &&
      !CI.getFrontendOpts().CodeCompletionAt.FileName.empty())
    CI.createCodeCompletionConsumer();

  // Use a code completion consumer?
  CodeCompleteConsumer *CompletionConsumer = nullptr;
  if (CI.hasCodeCompletionConsumer())
    CompletionConsumer = &CI.getCodeCompletionConsumer();

  if (!CI.hasSema())
    CI.createSema(getTranslationUnitKind(), CompletionConsumer);

  loadASTFiles();

  AdaptedAction->ExecuteAction();
  CI.getDiagnostics().getClient()->EndSourceFile();
}

const char *readerStatusToString(ASTReader::ASTReadResult Res) {
  // TODO Levitation: introduce string constants in .td file
  switch (Res) {
    case ASTReader::Success:
      return "Successfull??";

    case ASTReader::Failure:
      return "File seems to be currupted.";

    case ASTReader::Missing:
      return "File is missing.";

    case ASTReader::OutOfDate:
      return "File is out of date.";

    case ASTReader::VersionMismatch:
      return "The AST file was written by a different version of Clang";

    case ASTReader::ConfigurationMismatch:
      return "The AST file was written with a different language/target "
             " configuration.";

    case ASTReader::HadErrors:
      return "AST file has errors.";
  }
}

static void diagFailedToRead(
    DiagnosticsEngine &Diag,
    StringRef File,
    ASTReader::ASTReadResult Res
) {
  Diag.Report(diag::err_levitation_failed_to_read_pch)
  << File;
  (void)Res;
}

void LevitationBuildObjectAction::loadASTFiles() {

  CompilerInstance &CI = getCompilerInstance();

  CI.getDiagnostics().getClient()->BeginSourceFile(
      CI.getASTContext().getLangOpts()
  );

  CI.getDiagnostics().SetArgToStringFn(
      &FormatASTNodeDiagnosticArgument,
      &CI.getASTContext()
  );

  IntrusiveRefCntPtr<DiagnosticIDs> DiagIDs(
      CI.getDiagnostics().getDiagnosticIDs()
  );

  ASTImporterLookupTable LookupTable(
      *CI.getASTContext().getTranslationUnitDecl()
  );

  StringRef MainFile =
      getCurrentFileKind().getFormat() == InputKind::Precompiled ?
      getCurrentFile() :
      StringRef();

  IntrusiveRefCntPtr<LevitationModulesReader>
      Reader(new LevitationModulesReader(CI, MainFile));
  CI.getASTContext().setExternalSource(Reader);

  with(auto Opened = Reader->open()) {
    for (const auto &Dep : ASTFiles) {
      Reader->readDependency(
          Dep,
          /*On Fail do*/ diagFailedToRead
      );
    }
  }

  if (Reader->hasErrors())
    diagFailedToRead(CI.getDiagnostics(), MainFile, Reader->getStatus());
}

std::unique_ptr<ASTConsumer> LevitationBuildObjectAction::CreateASTConsumer(CompilerInstance &CI, StringRef InFile) {

  auto AdoptedConsumer = ASTMergeAction::CreateASTConsumer(CI, InFile);

  if (getCurrentFileKind().getFormat() != InputKind::Precompiled)
    return AdoptedConsumer;

  return MultiplexConsumerBuilder()
      .addNotNull(CreatePackageInstantiator())
      .addRequired(std::move(AdoptedConsumer))
  .done();
}

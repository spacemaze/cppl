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
#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Sema/TemplateInstCallback.h"
#include "clang/Serialization/ASTReader.h"
#include "clang/Serialization/ASTWriter.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
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

} // end of anonymous namespace

std::unique_ptr<ASTConsumer> LevitationBuildASTAction::CreateASTConsumer(
    clang::CompilerInstance &CI,
    llvm::StringRef InFile
) {
  std::vector<std::unique_ptr<ASTConsumer>> Consumers;

  auto ParserPostProcessor = CreateParserPostProcessor();
  auto DependenciesProcessor = CreateDependenciesASTProcessor(CI, InFile);
  auto AstFileCreator = CreateGeneratePCHConsumer(CI, InFile);

  if (!AstFileCreator)
    return nullptr;

  Consumers.push_back(std::move(ParserPostProcessor));
  Consumers.push_back(std::move(DependenciesProcessor));
  Consumers.push_back(std::move(AstFileCreator));

  return llvm::make_unique<MultiplexConsumer>(std::move(Consumers));
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

  importASTFiles();

  if (getCurrentFileKind().getFormat() == InputKind::Precompiled) {
    loadMainFile(getCurrentFile());
  }

  AdaptedAction->ExecuteAction();
  CI.getDiagnostics().getClient()->EndSourceFile();
}

void LevitationBuildObjectAction::importASTFiles() {

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

  // Import dependencies
  for (const auto &Dep : ASTFiles) {
    importAST(Dep, LookupTable, DiagIDs);
  }
}

void LevitationBuildObjectAction::importAST(
    StringRef ASTFile,
    ASTImporterLookupTable &LookupTable,
    IntrusiveRefCntPtr<DiagnosticIDs> DiagIDs
) {
  // ASTMerge::ExecuteAction has been used as a source,
  // a bit refactored though...

  CompilerInstance &CI = getCompilerInstance();

  IntrusiveRefCntPtr<DiagnosticsEngine> Diags(new DiagnosticsEngine(
      DiagIDs,
      &CI.getDiagnosticOpts(),
      new ForwardingDiagnosticConsumer(*CI.getDiagnostics().getClient()),
      /*ShouldOwnClient=*/true
  ));

  std::unique_ptr<ASTUnit> Unit = ASTUnit::LoadFromASTFile(
      ASTFile,
      CI.getPCHContainerReader(),
      ASTUnit::LoadEverything,
      Diags,
      CI.getFileSystemOpts(),
      /*UseDebugInfo=*/false,
      /*OnlyLocalDecls=*/false,
      /*RemappedFiles=*/None,
      /*CaptureDiagnostics=*/false,
      /*AllowPCHWithCompilerErrors=*/false,
      /*UserFilesAreVolatile=*/false,
      /*ReadDeclarationsOnly=*/true
  );

  if (!Unit)
    return;

  ASTImporter Importer(
      CI.getASTContext(),
      CI.getFileManager(),
      Unit->getASTContext(),
      Unit->getFileManager(),
      /*MinimalImport=*/false,
      &LookupTable
  );

  TranslationUnitDecl *TU = Unit->getASTContext().getTranslationUnitDecl();

  importTU(Importer, TU);
}

void LevitationBuildObjectAction::importTU(
    ASTImporter& Importer, TranslationUnitDecl *TU
) {

  CompilerInstance &CI = getCompilerInstance();

  for (auto *D : TU->decls()) {
    // Don't re-import __va_list_tag, __builtin_va_list.
    if (const auto *ND = dyn_cast<NamedDecl>(D))
      if (IdentifierInfo *II = ND->getIdentifier())
        if (II->isStr("__va_list_tag") || II->isStr("__builtin_va_list"))
          continue;

    llvm::Expected<Decl *> ToDOrError = Importer.Import_New(D);

    if (ToDOrError) {
      DeclGroupRef DGR(*ToDOrError);
      CI.getASTConsumer().HandleTopLevelDecl(DGR);
    } else {
      llvm::consumeError(ToDOrError.takeError());
    }
  }
}

void LevitationBuildObjectAction::loadMainFile(llvm::StringRef MainFile) {

  CompilerInstance &CI = getCompilerInstance();

  IntrusiveRefCntPtr<ASTReader> Reader(new ASTReader(
      CI.getPreprocessor(),
      CI.getModuleCache(),
      &CI.getASTContext(),
      CI.getPCHContainerReader(),
      {}
  ));

  // Attach the AST reader to the AST context as an external AST
  // source, so that declarations will be deserialized from the
  // AST file as needed.
  // We need the external source to be set up before we read the AST, because
  // eagerly-deserialized declarations may use it.
  CI.getASTContext().setExternalSource(Reader);

  switch (Reader->ReadAST(
      MainFile,
      serialization::MK_MainFile,
      SourceLocation(),
      ASTReader::ARR_None
  )) {
  case ASTReader::Success:
    CI.getPreprocessor().setPredefines(Reader->getSuggestedPredefines());
    return;

  case ASTReader::Failure:
  case ASTReader::Missing:
  case ASTReader::OutOfDate:
  case ASTReader::VersionMismatch:
  case ASTReader::ConfigurationMismatch:
  case ASTReader::HadErrors:
    CI.getDiagnostics().Report(diag::err_fe_unable_to_load_pch);
    CI.setExternalSemaSource(nullptr);
  }
}

std::unique_ptr<ASTConsumer> LevitationBuildObjectAction::CreateASTConsumer(CompilerInstance &CI, StringRef InFile) {

  auto AdoptedConsumer = ASTMergeAction::CreateASTConsumer(CI, InFile);

  if (getCurrentFileKind().getFormat() != InputKind::Precompiled)
    return AdoptedConsumer;

  if (!AdoptedConsumer)
    return nullptr;

  std::vector<std::unique_ptr<ASTConsumer>> Consumers;

  auto PackageInstantiator = CreatePackageInstantiator();

  assert(PackageInstantiator && "Failed to package instantiator?");

  Consumers.push_back(std::move(PackageInstantiator));
  Consumers.push_back(std::move(AdoptedConsumer));

  return llvm::make_unique<MultiplexConsumer>(std::move(Consumers));
}

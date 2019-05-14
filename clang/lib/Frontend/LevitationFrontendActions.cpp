//==-- LevitationFrontendActions.cpp --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Frontend/LevitationFrontendActions.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Basic/FileManager.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Frontend/LevitationASTConsumers.h"
#include "clang/Frontend/MultiplexConsumer.h"
#include "clang/Frontend/Utils.h"
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

typedef std::vector<std::unique_ptr<ASTConsumer>> ConsumersVector;

struct OutputFileHandle {
  std::unique_ptr<llvm::raw_pwrite_stream> OutputStream;
  std::string Path;

  static OutputFileHandle makeInvalid() {
    return {nullptr, ""};
  }

  bool isInvalid() const { return !(bool)OutputStream; };
};

enum ASTGeneratorKind {
  DefinitionGenerator,
  DeclarationGenerator
};

// Cloned with some alterations from GeneratePCHAction.
OutputFileHandle CreateOutputFile(
    ASTGeneratorKind GeneratorKind,
    CompilerInstance &CI,
    StringRef InFile
) {
  // Comment from original method:
  // We use createOutputFile here because this is exposed via libclang, and we
  // must disable the RemoveFileOnSignal behavior.
  // We use a temporary to avoid race conditions.

  StringRef Extension = "";
  SmallString<256> OutputPath(CI.getFrontendOpts().OutputFile);

  if (GeneratorKind == DeclarationGenerator) {
    Extension = CI.getFrontendOpts().LevitationDeclASTFileExtension;
    if (!OutputPath.empty())
      llvm::sys::path::replace_extension(OutputPath, Extension);
  }

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
    ASTGeneratorKind GeneratorKind,
    CompilerInstance &CI,
    StringRef InFile
) {
  OutputFileHandle OutputFile = CreateOutputFile(
      GeneratorKind,
      CI,
      InFile
  );

  if (OutputFile.isInvalid())
    return nullptr;

  const auto &FrontendOpts = CI.getFrontendOpts();
  auto Buffer = std::make_shared<PCHBuffer>();
  std::vector<std::unique_ptr<ASTConsumer>> Consumers;

  bool EmitDeclarationOnly = GeneratorKind == DeclarationGenerator;

  Consumers.push_back(llvm::make_unique<PCHGenerator>(
      CI.getPreprocessor(),
      CI.getModuleCache(),
      OutputFile.Path,
      "",
      Buffer,
      FrontendOpts.ModuleFileExtensions,
      CI.getPreprocessorOpts().AllowPCHWithCompilerErrors,
      FrontendOpts.IncludeTimestamps,
      +CI.getLangOpts().CacheGeneratedPCH,
      EmitDeclarationOnly
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

std::unique_ptr<ASTConsumer> LevitationBuildASTAction::CreateASTConsumer(
    clang::CompilerInstance &CI,
    llvm::StringRef InFile
) {
  std::vector<std::unique_ptr<ASTConsumer>> Consumers;

  auto DependenciesProcessor = CreateDependenciesASTProcessor(CI, InFile);
  auto DeclCreator = CreateGeneratePCHConsumer(DeclarationGenerator, CI, InFile);
  auto DefCreator = CreateGeneratePCHConsumer(DefinitionGenerator, CI, InFile);

  assert(DependenciesProcessor && "Failed to create dependencies processor?");

  if (!DefCreator)
    return nullptr;

  Consumers.push_back(std::move(DependenciesProcessor));
  Consumers.push_back(std::move(DeclCreator));
  Consumers.push_back(std::move(DefCreator));

  return llvm::make_unique<MultiplexConsumer>(std::move(Consumers));
}
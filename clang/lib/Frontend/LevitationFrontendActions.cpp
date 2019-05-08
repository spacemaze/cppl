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

using namespace clang;

typedef std::vector<std::unique_ptr<ASTConsumer>> ConsumersVector;

std::unique_ptr<ASTConsumer> LevitationBuildASTAction::CreateASTConsumer(
    clang::CompilerInstance &CI,
    llvm::StringRef InFile
) {
  std::vector<std::unique_ptr<ASTConsumer>> Consumers;

  auto DependenciesProcessor = CreateDependenciesASTProcessor(CI);
  auto ASTFileCreator = GeneratePCHAction::CreateASTConsumer(CI, InFile);

  assert(DependenciesProcessor && "Failed to create dependencies processor?");
  if (!ASTFileCreator)
    return nullptr;

  Consumers.push_back(std::move(DependenciesProcessor));
  Consumers.push_back(std::move(ASTFileCreator));

  return llvm::make_unique<MultiplexConsumer>(std::move(Consumers));
}
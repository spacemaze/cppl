//==-- LevitationFrontendActions.h - Levitation Frontend Actions -*- C++ -*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_FRONTEND_LEVITATIONFRONTENDACTIONS_H
#define LLVM_CLANG_FRONTEND_LEVITATIONFRONTENDACTIONS_H

#include "clang/Frontend/FrontendActions.h"
#include "clang/CodeGen/CodeGenAction.h"
#include <string>
#include <vector>

namespace clang {

class ASTImporter;
class ASTImporterLookupTable;
class TranslationUnitDecl;

// FIXME Levitation: move into levitation namespace
// FIXME Levitation: move into Levitation directory
class LevitationBuildASTAction : public GeneratePCHAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(
      CompilerInstance &CI,
      StringRef InFile
  ) override;
};

class LevitationBuildObjectAction : public ASTMergeAction {

public:

  LevitationBuildObjectAction(
      std::unique_ptr<FrontendAction> &&AdaptedAction,
      ArrayRef<std::string> DependencyASTs
  ) : ASTMergeAction(std::move(AdaptedAction), DependencyASTs)
  {}

  /// 1. Completes infrastructure for final AST, at this stage we should get created:
  ///
  /// Created during FrontendAction::BeginSourceFile:
  /// * FileManager
  /// * SourceManager (initialized)
  /// * Preprocessor (with initialized builtins?)
  /// * ASTContext
  /// Created by ExecuteAction itself:
  /// * CodeCompletion consumer (if any)
  /// * Sema
  ///
  /// 2. Imports C++ Levitation dependencies (if any) by means of ASTImporter
  /// 3. Adds main AST contents:
  /// * If input file is AST, it is also to be loaded
  ///   (directly into main context)
  ///
  /// * If input file is code, parse it.
  void ExecuteAction() override;

  bool usesPreprocessorOnly() const override { return false; }

protected:
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef InFile) override;

  void importASTFiles();
};

}  // end namespace clang

#endif

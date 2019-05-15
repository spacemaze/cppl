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
// FIXME Levitation: move into levitation namespace
// FIXME Levitation: move into Levitation directory

class LevitationBuildASTAction : public GeneratePCHAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(
      CompilerInstance &CI,
      StringRef InFile
  ) override;
};

/**
 * Frontend action adaptor that merges ASTs together.
 *
 * This action takes an existing AST file and "merges" it into the AST
 * context, producing a merged context. This action is an action
 * adaptor, which forwards most of its calls to another action that
 * will consume the merged context.
 */
class MergeASTDependenciesAction : public ASTMergeAction {
protected:
  void ExecuteAction() override;
public:
    MergeASTDependenciesAction(
        std::unique_ptr<FrontendAction> AdaptedAction,
        ArrayRef<std::string> ASTFiles
    ) :
    ASTMergeAction(std::move(AdaptedAction), ASTFiles)
    {}
};

}  // end namespace clang

#endif

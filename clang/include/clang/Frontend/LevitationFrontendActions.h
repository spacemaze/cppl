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

class LevitationBuildASTAction : public GeneratePCHAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(
      CompilerInstance &CI,
      StringRef InFile
  ) override;
};

}  // end namespace clang

#endif

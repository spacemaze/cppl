//===-- LevitationASTConsumers.h - Levitation AST Consumers -*- C++ -----*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_FRONTEND_LEVITATIONASTCONSUMERS_H
#define LLVM_CLANG_FRONTEND_LEVITATIONASTCONSUMERS_H

#include "clang/Basic/LLVM.h"
#include <memory>

namespace clang {

class ASTConsumer;
class CompilerInstance;

std::unique_ptr<ASTConsumer> CreateDependenciesASTProcessor(
    CompilerInstance &CI,
    StringRef InFile
);

std::unique_ptr<ASTConsumer> CreatePackageInstantiator(CompilerInstance &CI);

}

#endif

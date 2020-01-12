//===--- C++ Levitation Parser.h --------------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines helper functions for clang::Parser class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_PARSER_H
#define LLVM_CLANG_LEVITATION_PARSER_H

#include <functional>

namespace clang {
    class Preprocessor;

namespace levitation {

void SkipFunctionBody(
    Preprocessor &PP,
    std::function<void()> &&SkipFunctionBody
);

}
}

#endif
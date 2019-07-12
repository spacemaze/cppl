//===--- C++ Levitation FileExtensions.h ------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines standard C++ Levitation files extensions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_FILEEXTENSIONS_H
#define LLVM_CLANG_LEVITATION_FILEEXTENSIONS_H

namespace clang {
namespace levitation {

struct FileExtensions {
  static constexpr char SourceCode [] = "cppl";
  static constexpr char Object [] = "o";
  static constexpr char DeclarationAST [] = "decl-ast";
  static constexpr char ParsedAST [] = "ast";
  static constexpr char ParsedDependencies [] = "ldeps";
  static constexpr char DirectDependencies [] = "d";
  static constexpr char FullDependencies [] = "fulld";
};

}
}

#endif //LLVM_CLANG_LEVITATION_FILEEXTENSIONS_H

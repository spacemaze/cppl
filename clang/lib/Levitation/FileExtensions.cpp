//===--- C++ Levitation FileExtensions.cpp ------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file is a required initializers set
//  for C++ Levitation FileExtensions structure.
//
//===----------------------------------------------------------------------===//

#include "clang/Levitation/FileExtensions.h"

namespace clang {
namespace levitation {
  constexpr char FileExtensions::SourceCode[];
  constexpr char FileExtensions::Object[];
  constexpr char FileExtensions::DeclarationAST[];
  constexpr char FileExtensions::ParsedAST[];
  constexpr char FileExtensions::ParsedDependencies[];
  constexpr char FileExtensions::DirectDependencies[];
  constexpr char FileExtensions::FullDependencies[];

}
}

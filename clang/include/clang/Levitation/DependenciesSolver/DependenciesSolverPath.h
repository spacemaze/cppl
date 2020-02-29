//===--- C++ Levitation DependenciesSolverPath.h --------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines C++ Levitation DependenciesSolver Path utilities.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_DEPENDENCIESSOLVERPATH_H
#define LLVM_CLANG_LEVITATION_DEPENDENCIESSOLVERPATH_H

#include "llvm/Support/Path.h"
#include "clang/Levitation/Common/Path.h"
#include "clang/Levitation/FileExtensions.h"

namespace clang { namespace levitation { namespace dependencies_solver {

/*static*/
class DependenciesSolverPath {
public:

  static void addDepPathsFor(
      Paths &Dst,
      StringRef BuildRoot,
      StringRef Package,
      bool MainFile = false
  ) {
    Dst.emplace_back(levitation::Path::getPath<SinglePath>(
        BuildRoot, Package, FileExtensions::DeclarationAST
    ));
  }
  static void addIncPathsFor(
      Paths &Dst,
      StringRef BuildRoot,
      StringRef Package,
      bool MainFile = false
  ) {
    Dst.emplace_back(levitation::Path::replaceExtension<SinglePath>(
        Package, FileExtensions::Header
    ));
  }

};

}}}

#endif //LLVM_CLANG_LEVITATION_DEPENDENCIESSOLVERPATH_H

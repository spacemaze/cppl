//===--- Dependencies.h - C++ Levitation Depencency classes -----*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines C++ Levitation PackageDependency class and
//  its satellites.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_DEPENDENCIES_H
#define LLVM_CLANG_LEVITATION_DEPENDENCIES_H

#include "clang/Basic/SourceLocation.h"
#include "clang/Levitation/Common/StringsPool.h"
#include "clang/Levitation/Common/Path.h"
#include "clang/Levitation/Common/IndexedSet.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"

namespace llvm {
  class raw_ostream;
}

namespace clang {

  class DiagnosticBuilder;
  class NamedDecl;

namespace levitation {

  /// Describes whole set of dependencies used by
  /// given package.
  struct PackageDependencies {
    PathsPoolTy PathsPool;

    /**
     * Declaration dependencies.
     * Set of dependencies declaration depends on.
     * If dependency is met only in function body whith external
     * linkage, then this is not a declaration dependency. It would be
     * a definition dependency.
     */
    PathIDsSet DeclarationDependencies;

    /**
     * Set of dependencies definition depends on.
     */
    PathIDsSet DefinitionDependencies;

    /**
     * Indicates, that current file is to be published
     * during C++ Levitation library creation.
     */
    bool IsPublic;

    /**
     * Indicates, that current file has no declaration part,
     * so no need to produce .decl-ast file, only .o file.
     */
    bool IsBodyOnly;

    /**
     * Unifies access to PathsPool
     * @return
     */
    PathsPoolTy& accessPathsPool() {
      return PathsPool;
    }

    void addDeclarationPath(StringRef Path) {
      auto ID = accessPathsPool().addItem(Path);
      DeclarationDependencies.insert(ID);
    }

    void addDefinitionPath(StringRef Path) {
      auto ID = accessPathsPool().addItem(Path);
      DefinitionDependencies.insert(ID);
    }
  };
}
}

#endif
//===--------------------- DependenciesSolverPathUtils.h --------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file path utils for dependencies solver tool
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LEVITATION_DEPENDENCIESSOLVERPATHUTILS_H
#define LLVM_LEVITATION_DEPENDENCIESSOLVERPATHUTILS_H

#include "clang/Levitation/Common/Path.h"
#include "llvm/ADT/StringRef.h"

namespace clang { namespace levitation { namespace dependencies_solver {

/*static*/
class DependenciesSolverPathUtils {
public:
    static SinglePath getPath(StringRef NewParent, StringRef SrcRel, StringRef Extension);
};

}}} // end of clang::levitation::dependencies_solver namespace

#endif //LLVM_LEVITATION_DEPENDENCIESSOLVERPATHUTILS_H

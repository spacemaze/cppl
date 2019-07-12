//===--- C++ Levitation DependenciesSolver.h --------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines C++ Levitation DependenciesSolver tool interface.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_DEPENDENCIESSOLVER_H
#define LLVM_CLANG_LEVITATION_DEPENDENCIESSOLVER_H

#include "clang/Levitation/Failable.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include <memory>

namespace llvm {
  class raw_ostream;
}

namespace clang {
  class FileManager;
}

namespace clang { namespace levitation {

class DependenciesSolver : public Failable {
  llvm::StringRef SourcesRoot;
  llvm::StringRef BuildRoot;
  llvm::StringRef MainFile;
  bool Verbose = false;
public:

  void setVerbose(bool Verbose) {
    DependenciesSolver::Verbose = Verbose;
  }

  void setSourcesRoot(llvm::StringRef SourcesRoot) {
    DependenciesSolver::SourcesRoot = SourcesRoot;
  }

  void setBuildRoot(llvm::StringRef BuildRoot) {
    DependenciesSolver::BuildRoot = BuildRoot;
  }

  void setMainFile(llvm::StringRef MainFile) {
    DependenciesSolver::MainFile = MainFile;
  }

  bool solve();

  friend class DependenciesSolverImpl;
};

}}

#endif //LLVM_CLANG_LEVITATION_DEPENDENCIESSOLVER_H

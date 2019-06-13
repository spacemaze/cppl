// TODO Levitation: Licensing

#ifndef LLVM_CLANG_LEVITATION_DEPENDENCIESSOLVER_H
#define LLVM_CLANG_LEVITATION_DEPENDENCIESSOLVER_H

#include "clang/Basic/FileManager.h"

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
  bool Verbose = false;
  FileManager FileMgr;
public:

  DependenciesSolver() : FileMgr( { /*Working dir*/ StringRef()} ) {}

  void setVerbose(bool Verbose) {
    DependenciesSolver::Verbose = Verbose;
  }

  void setSourcesRoot(llvm::StringRef SourcesRoot) {
    DependenciesSolver::SourcesRoot = SourcesRoot;
  }

  void setBuildRoot(llvm::StringRef BuildRoot) {
    DependenciesSolver::BuildRoot = BuildRoot;
  }

  bool solve();

  friend class DependenciesSolverImplementation;
};

}}

#endif //LLVM_CLANG_LEVITATION_DEPENDENCIESSOLVER_H

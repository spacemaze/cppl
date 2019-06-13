// TODO Levitation: Licensing

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

  bool solve();

  friend class DependenciesSolverImpl;
};

}}

#endif //LLVM_CLANG_LEVITATION_DEPENDENCIESSOLVER_H

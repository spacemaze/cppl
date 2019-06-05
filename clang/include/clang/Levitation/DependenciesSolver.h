// TODO Levitation: Licensing

#ifndef LLVM_CLANG_LEVITATION_DEPENDENCIESSOLVER_H
#define LLVM_CLANG_LEVITATION_DEPENDENCIESSOLVER_H

#include "llvm/ADT/StringRef.h"

namespace llvm {
  class raw_ostream;
}

namespace clang { namespace levitation {

class DependenciesSolver {
  llvm::StringRef DirectDepsRoot;
  llvm::StringRef DepsOutput;
  bool Verbose = false;
public:

  void setVerbose(bool Verbose) {
    DependenciesSolver::Verbose = Verbose;
  }

  void setDirectDepsRoot(llvm::StringRef DirectDepsRoot) {
    DependenciesSolver::DirectDepsRoot = DirectDepsRoot;
  }

  void setDepsOutput(llvm::StringRef DepsOutput) {
    DependenciesSolver::DepsOutput = DepsOutput;
  }

  void solve();

protected:
  llvm::raw_ostream &verbose();
};

}}

#endif //LLVM_CLANG_LEVITATION_DEPENDENCIESSOLVER_H

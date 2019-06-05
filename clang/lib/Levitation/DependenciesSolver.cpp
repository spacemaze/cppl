// TODO Levitation: Licensing

#include "clang/Levitation/DependenciesSolver.h"
#include "llvm/Support/raw_ostream.h"

namespace clang { namespace levitation {

void DependenciesSolver::solve() {
  verbose()
  << "Running Dependencies Solver\n"
  << "Root: " << DirectDepsRoot << "\n"
  << "Output root: " << DepsOutput << "\n";
}

llvm::raw_ostream& DependenciesSolver::verbose() {
  return Verbose ? llvm::outs() : llvm::nulls();
}

}}

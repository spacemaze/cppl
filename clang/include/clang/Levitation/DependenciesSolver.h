// TODO Levitation: Licensing

#ifndef LLVM_CLANG_LEVITATION_DEPENDENCIESSOLVER_H
#define LLVM_CLANG_LEVITATION_DEPENDENCIESSOLVER_H

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

struct PackageDependencies;

using Paths = llvm::SmallVector<llvm::SmallString<256>, 64>;

using ParsedDependenciesVector = Paths;
using ParsedDependencies = llvm::DenseMap<llvm::StringRef, std::unique_ptr<PackageDependencies>>;
class DependenciesDAG;
class SolvedDependenciesInfo;
using SolvedDependenciesMap = llvm::DenseMap<llvm::StringRef, std::unique_ptr<SolvedDependenciesInfo>>;

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

  void collectParsedDependencies(ParsedDependenciesVector &Dest);

  void  loadDependencies(
      ParsedDependencies &Dest,
      const ParsedDependenciesVector &ParsedDepFiles
  );

  void writeResult(const SolvedDependenciesMap &SolvedDependencies);
};

}}

#endif //LLVM_CLANG_LEVITATION_DEPENDENCIESSOLVER_H

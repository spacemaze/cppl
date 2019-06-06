// TODO Levitation: Licensing

#include "clang/Basic/FileManager.h"

#include "clang/Levitation/Dependencies.h"
#include "clang/Levitation/DependenciesSolver.h"
#include "clang/Levitation/WithOperator.h"
#include "clang/Levitation/FileExtensions.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/FileSystem.h"

#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace clang { namespace levitation {

namespace {

class DumpAction : public WithOperand {
  llvm::raw_ostream &Out;
  bool Failed = false;
  unsigned NumPageBreaks;
public:
  DumpAction(raw_ostream &Out, const char *Title, unsigned NumPageBreaks = 1)
  : Out(Out),
    NumPageBreaks(NumPageBreaks) {
    Out << Title << "... ";
  }

  ~DumpAction() {
    Out << "complete.";
    for (unsigned i = 0; i != NumPageBreaks; ++i)
      Out << "\n";
  }

  void setFailed() { DumpAction::Failed = true; }

  llvm::raw_ostream &operator()() { return Out; }
};

} // end of anonymous namespace

class DependenciesDAG {
public:
  static std::unique_ptr<DependenciesDAG> build(
      ParsedDependencies &ParsedDeps,
      llvm::raw_ostream &out
  ) {
    with (auto A = DumpAction(out, "Building dependencies DAG")) {
    }

    return nullptr;
  }
};

class SolvedDependenciesInfo {
public:
  static std::unique_ptr<SolvedDependenciesMap> buildFromDAG(
      const DependenciesDAG &DDAG,
      llvm::raw_ostream &out
  ) {
    with (auto A = DumpAction(out, "Solving dependencies")) {
    }

    return nullptr;
  }
};

void DependenciesSolver::solve() {
  verbose()
  << "Running Dependencies Solver\n"
  << "Root: " << DirectDepsRoot << "\n"
  << "Output root: " << DepsOutput << "\n\n";

  ParsedDependenciesVector ParsedDepFiles;
  collectParsedDependencies(ParsedDepFiles);

  verbose()
  << "Found " << ParsedDepFiles.size()
  << " '." << FileExtensions::DirectDependencies << "' files.\n\n";

  ParsedDependencies parsedDependencies;
  loadDependencies(parsedDependencies, ParsedDepFiles);

  auto DDAG = DependenciesDAG::build(parsedDependencies, verbose());

  auto SolvedDependencies = SolvedDependenciesInfo::buildFromDAG(*DDAG, verbose());


  writeResult(*SolvedDependencies);

}

llvm::raw_ostream& DependenciesSolver::verbose() {
  return Verbose ? llvm::outs() : llvm::nulls();
}

void collectFilesWithExtension(
    ParsedDependenciesVector &Dest,
    Paths &NewSubDirs,
    llvm::vfs::FileSystem &FS,
    StringRef CurDir,
    StringRef FileExtension
) {
  std::error_code EC;

  for (
    llvm::vfs::directory_iterator Dir = FS.dir_begin(CurDir, EC), e;
    Dir != e && !EC;
    Dir.increment(EC)
  ) {
    StringRef Path = Dir->path();

    switch (Dir->type()) {
      case llvm::sys::fs::file_type::regular_file:
        if (llvm::sys::path::extension(Path) == FileExtension)
          Dest.push_back(Path);
      break;

      case llvm::sys::fs::file_type::directory_file:
        NewSubDirs.push_back(Path);
      break;

      default:
      break;
    }
  }
}

void DependenciesSolver::collectParsedDependencies(
    ParsedDependenciesVector &Dest
) {
  with(auto A = DumpAction(verbose(), "Collecting dependencies")) {
    FileManager FileMgr({ /*Working dir*/ StringRef()});

    auto &FS = FileMgr.getVirtualFileSystem();

    Paths SubDirs;
    SubDirs.push_back(DirectDepsRoot);

    std::string parsedDepsFileExtension = ".";
    parsedDepsFileExtension += FileExtensions::DirectDependencies;

    Paths NewSubDirs;
    while (SubDirs.size()) {
      NewSubDirs.clear();
      for (StringRef CurDir : SubDirs) {
        collectFilesWithExtension(
            Dest,
            NewSubDirs,
            FS,
            CurDir,
            parsedDepsFileExtension
        );
      }
      SubDirs.swap(NewSubDirs);
    }
  }
}

void DependenciesSolver::loadDependencies(
    ParsedDependencies &Dest,
    const ParsedDependenciesVector &ParsedDepFiles
) {
  with (auto A = DumpAction(verbose(), "Loading dependencies info")) {

  }
}

void DependenciesSolver::writeResult(
    const SolvedDependenciesMap &SolvedDependencies
) {
  with (auto A = DumpAction(verbose(), "Generating dependency files")) {

  }
}

}}

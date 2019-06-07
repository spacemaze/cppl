// TODO Levitation: Licensing

#include "clang/Levitation/Dependencies.h"
#include "clang/Levitation/DependenciesSolver.h"
#include "clang/Levitation/WithOperator.h"
#include "clang/Levitation/File.h"
#include "clang/Levitation/FileExtensions.h"
#include "clang/Levitation/Serialization.h"
#include "clang/Levitation/StringsPool.h"

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


class ParsedDependencies {
  DependenciesStringsPool Strings;

  using DependenciesMap = llvm::DenseMap<StringID, std::unique_ptr<DependenciesData>>;
  DependenciesMap Map;

public:

  void add(const DependenciesData &Deps) {
    auto NewDeps = llvm::make_unique<DependenciesData>(&Strings);

    DenseMap<StringID, StringID> OldToNew;

    for (auto &OldS : Deps.Strings->items()) {
      auto NewID = Strings.addItem(OldS.second);
      OldToNew[OldS.first] = NewID;
    }

    NewDeps->PackageFilePathID = OldToNew[Deps.PackageFilePathID];

    for (auto &DeclDep : Deps.DeclarationDependencies) {
      NewDeps->DeclarationDependencies.emplace_back(
          DependenciesData::Declaration {
              OldToNew[DeclDep.FilePathID],
              DeclDep.LocationIDBegin,
              DeclDep.LocationIDEnd
          }
      );
    }

    for (auto &DeclDep : Deps.DefinitionDependencies) {
      NewDeps->DefinitionDependencies.emplace_back(
          DependenciesData::Declaration {
              OldToNew[DeclDep.FilePathID],
              DeclDep.LocationIDBegin,
              DeclDep.LocationIDEnd
          }
      );
    }

    auto InsertionRes =
        Map.insert({ NewDeps->PackageFilePathID, std::move(NewDeps) });

    assert(
        InsertionRes.second && "Loaded dependencies has been already added"
    );
  }

  DependenciesMap::const_iterator begin() const { return Map.begin(); }
  DependenciesMap::const_iterator end() const { return Map.end(); }
};

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
};

using SolvedDependenciesMap = llvm::DenseMap<llvm::StringRef, std::unique_ptr<SolvedDependenciesInfo>>;

static std::unique_ptr<SolvedDependenciesMap> buildSolvedDependencies(
    const DependenciesDAG &DDAG,
    llvm::raw_ostream &out
) {
  with (auto A = DumpAction(out, "Solving dependencies")) {
  }

  return nullptr;
}

using Paths = llvm::SmallVector<llvm::SmallString<256>, 64>;
using ParsedDependenciesVector = Paths;

class DependenciesSolverHelper {
  DependenciesSolver *Solver;
public:
  DependenciesSolverHelper(DependenciesSolver *solver) : Solver(solver) {}
  static void collectFilesWithExtension(
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

  void collectParsedDependencies(
      ParsedDependenciesVector &Dest
  ) {
    with(auto A = DumpAction(Solver->verbose(), "Collecting dependencies")) {
      auto &FS = Solver->FileMgr.getVirtualFileSystem();

      Paths SubDirs;
      SubDirs.push_back(Solver->DirectDepsRoot);

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

  static bool loadFromBuffer(
      llvm::raw_ostream &diags,
      ParsedDependencies &Dest,
      const llvm::MemoryBuffer &MemBuf,
      StringRef PackagePath
  ) {
    auto Reader = CreateBitstreamReader(MemBuf);

    DependenciesData Dependencies;

    if (!Reader->read(Dependencies)) {
      diags << Reader->getErrorMessage() << "\n";
      return false;
    }

    Dest.add(Dependencies);

    return true;
  }

  void loadDependencies(
      ParsedDependencies &Dest,
      const ParsedDependenciesVector &ParsedDepFiles
  ) {
    with (auto A = DumpAction(Solver->verbose(), "Loading dependencies info")) {
      for (StringRef PackagePath : ParsedDepFiles) {
        if (auto Buffer = Solver->FileMgr.getBufferForFile(PackagePath)) {
          llvm::MemoryBuffer &MemBuf = *Buffer.get();

          if (!loadFromBuffer(Solver->error(), Dest, MemBuf, PackagePath)) {
            // TODO Levitation: Do something with errors loggin and DumpAction
            //   it is awful.
            A.setFailed();
            Solver->error()
            << "Failed to read dependencies for '" << PackagePath << "'\n";
          }
        } else {
         A.setFailed();
         Solver->error() << "Failed to open file '" << PackagePath << "'\n";
        }
      }
    }
  }

  void dump(const ParsedDependencies &ParsedDependencies) {
    for (auto &PackageDependencies : ParsedDependencies) {
      DependenciesData &Data = *PackageDependencies.second;
      auto &Strings = *Data.Strings;
      Solver->verbose()
      << "Package: " << *Strings.getItem(Data.PackageFilePathID) << "\n";

      if (
        Data.DeclarationDependencies.empty() &&
        Data.DefinitionDependencies.empty()
      ) {
        Solver->verbose()
        << "    no dependencies.\n";
      } else {
        dump(4, "Declaration depends on:", Strings, Data.DeclarationDependencies);
        dump(4, "Definition depends on:", Strings, Data.DefinitionDependencies);
      }

      Solver->verbose() << "\n";
    }
  }

  void dump(
      unsigned Indent,
      StringRef Title,
      DependenciesStringsPool &Strings,
      DependenciesData::DeclarationsBlock &Deps
  ) {
    if (Deps.empty())
      return;

    Solver->verbose().indent(Indent) << Title << "\n";
    for (auto &Dep : Deps) {
        Solver->verbose().indent(Indent + 4)
        << *Strings.getItem(Dep.FilePathID) << "\n";
    }
  }

  void writeResult(
      const SolvedDependenciesMap &SolvedDependencies
  ) {
    with (auto A = DumpAction(Solver->verbose(), "Generating dependency files")) {

    }
  }

};

void DependenciesSolver::solve() {
  verbose()
  << "Running Dependencies Solver\n"
  << "Root: " << DirectDepsRoot << "\n"
  << "Output root: " << DepsOutput << "\n\n";

  DependenciesSolverHelper Helper(this);

  ParsedDependenciesVector ParsedDepFiles;
  Helper.collectParsedDependencies(ParsedDepFiles);

  verbose()
  << "Found " << ParsedDepFiles.size()
  << " '." << FileExtensions::DirectDependencies << "' files.\n\n";

  ParsedDependencies parsedDependencies;
  Helper.loadDependencies(parsedDependencies, ParsedDepFiles);

  Helper.dump(parsedDependencies);

  auto DDAG = DependenciesDAG::build(parsedDependencies, verbose());

  auto SolvedDependencies = buildSolvedDependencies(*DDAG, verbose());

  Helper.writeResult(*SolvedDependencies);

}

llvm::raw_ostream &DependenciesSolver::verbose() {
  return Verbose ? llvm::outs() : llvm::nulls();
}
llvm::raw_ostream &DependenciesSolver::error() {
  return llvm::errs();
}
}}

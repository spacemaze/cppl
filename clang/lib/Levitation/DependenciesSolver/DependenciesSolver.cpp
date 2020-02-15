//===--- C++ Levitation DependenciesSolver.h --------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines C++ Levitation DependenciesSolver tool implementation.
//
//===----------------------------------------------------------------------===//

#include "clang/Levitation/Common/CreatableSingleton.h"
#include "clang/Levitation/Common/File.h"
#include "clang/Levitation/Common/FileSystem.h"
#include "clang/Levitation/Common/Path.h"
#include "clang/Levitation/Common/SimpleLogger.h"
#include "clang/Levitation/Common/StringsPool.h"
#include "clang/Levitation/Common/WithOperator.h"
#include "clang/Levitation/Dependencies.h"
#include "clang/Levitation/DependenciesSolver/DependenciesGraph.h"
#include "clang/Levitation/DependenciesSolver/SolvedDependenciesInfo.h"
#include "clang/Levitation/DependenciesSolver/DependenciesSolverPath.h"
#include "clang/Levitation/DependenciesSolver/DependenciesSolver.h"
#include "clang/Levitation/FileExtensions.h"
#include "clang/Levitation/Serialization.h"

#include "clang/Basic/FileManager.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

#include <algorithm>

using namespace llvm;

namespace clang { namespace levitation { namespace dependencies_solver {

// This predeclaration is required for proper "friendship" with its
// preceeding user DependenciesSolverContext
class DependenciesSolverImpl;

namespace {

//=============================================================================
// Dependencies Solver Context

class DependenciesSolverContext {

  DependenciesSolver &Solver;

  // TODO Levitation: make it singleton?
  DependenciesStringsPool &StringsPool;

  const Paths &LDepsFiles;

  std::shared_ptr<ParsedDependencies> ParsedDeps;
  std::shared_ptr<DependenciesGraph> DepsGraph;
  std::shared_ptr<SolvedDependenciesInfo> SolvedDepsInfo;

  friend class clang::levitation::dependencies_solver::DependenciesSolverImpl;

public:

  DependenciesSolverContext(
      DependenciesSolver &Solver,
      const Paths &ldepsFiles
  )
  : Solver(Solver),
    StringsPool(CreatableSingleton<DependenciesStringsPool >::get()),
    LDepsFiles(ldepsFiles)
  {}

  DependenciesStringsPool &getStringsPool() {
    return StringsPool;
  }

  const ParsedDependencies &getParsedDependencies() const {
    assert(ParsedDeps && "ParsedDependencies should be set");
    return *ParsedDeps;
  }

  std::shared_ptr<DependenciesGraph> &&detachDependenciesGraph() {
    assert(DepsGraph && "DependenciesGraph should be set");
    return std::move(DepsGraph);
  }

  // TODO Levitation: probably pick up better name.
  std::shared_ptr<SolvedDependenciesInfo> detachSolvedDependenciesInfo() {
    assert(SolvedDepsInfo && "SolvedDependenciesInfo should be set");
    return std::move(SolvedDepsInfo);
  }

  const SolvedDependenciesInfo &getSolvedDependenciesInfo() const {
    assert(SolvedDepsInfo && "SolvedDependenciesInfo should be set");
    return *SolvedDepsInfo;
  }
};

} // end of anonymous namespace

using SolvedDependenciesMap = llvm::DenseMap<llvm::StringRef, std::unique_ptr<SolvedDependenciesInfo>>;

class DependenciesSolverImpl {
  DependenciesSolverContext &Context;
  log::Logger &Log = log::Logger::get();
  DependenciesSolver *Solver;

public:
  DependenciesSolverImpl(
      DependenciesSolverContext &Context
  ) : Context(Context), Solver(&Context.Solver) {}

  DependenciesSolverContext &getContext() { return Context; }

  static void collectFilesWithExtension(
      Paths &Dest,
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

  static Paths collectLDepsFiles(llvm::StringRef BuildRoot) {

    Paths LDepsFiles;
    auto &Log = log::Logger::get();

    Log.verbose() << "Collecting dependencies...\n";

    auto &FM = CreatableSingleton<FileManager>::get();
    auto &FS = FM.getVirtualFileSystem();

    Paths SubDirs;
    SubDirs.push_back(BuildRoot);

    std::string parsedDepsFileExtension = ".";
    parsedDepsFileExtension += FileExtensions::ParsedDependencies;

    Paths NewSubDirs;
    while (SubDirs.size()) {
      NewSubDirs.clear();
      for (StringRef CurDir : SubDirs) {
        collectFilesWithExtension(
            LDepsFiles,
            NewSubDirs,
            FS,
            CurDir,
            parsedDepsFileExtension
        );
      }
      SubDirs.swap(NewSubDirs);
    }

    Log.verbose()
    << "Found " << LDepsFiles.size()
    << " '." << FileExtensions::ParsedDependencies << "' files.\n\n";

    return LDepsFiles;
  }

  void collectParsedDependencies() {
    loadDependencies(Context.LDepsFiles);

    Log.verbose()
    << "Loaded dependencies:\n";
    dump(Log.verbose(), Context.getParsedDependencies());
  }

  bool sourceExists(StringRef OriginalSourceRel) {
    PathString OriginalSourceFull = Context.Solver.SourcesRoot;
    llvm::sys::path::append(OriginalSourceFull, OriginalSourceRel);

    return llvm::sys::fs::exists(OriginalSourceFull);
  }

  bool loadFromBuffer(
      ParsedDependencies &Dest,
      const llvm::MemoryBuffer &MemBuf
  ) {
    auto &Log = log::Logger::get();

    auto Reader = CreateBitstreamReader(MemBuf);

    DependenciesData PackageData;

    if (!Reader->read(PackageData)) {
      Log.error() << Reader->getStatus().getErrorMessage() << "\n";
      return false;
    }

    if (Reader->getStatus().hasWarnings()) {
      Log.warning() << Reader->getStatus().getWarningMessage() << "\n";
    }

    const auto &Source =
        *PackageData.Strings->getItem(PackageData.PackageFilePathID);

    if (llvm::sys::path::is_absolute(Source)) {
      Log.error()
          << "Source should not be absolute:\n"
          << "    " << Source;
      return false;
    }

    if (sourceExists(Source))
      Dest.add(PackageData);
    else
      Log.warning()
      << "Skipping parsed dependency, since its source was removed:\n"
      << "    " << Source << "\n";

    return true;
  }

  void loadDependencies(const Paths &ParsedDepFiles) {
    Context.ParsedDeps = std::make_shared<ParsedDependencies>(
        Context.getStringsPool()
    );
    ParsedDependencies &Dest = *Context.ParsedDeps;

    Log.verbose() << "Loading dependencies info...\n";

    auto &FM = CreatableSingleton<FileManager>::get();

    for (StringRef PackagePath : ParsedDepFiles) {
      if (auto Buffer = FM.getBufferForFile(PackagePath)) {
        llvm::MemoryBuffer &MemBuf = *Buffer.get();

        if (!loadFromBuffer(Dest, MemBuf)) {
          // TODO Levitation: Do something with errors logging and DumpAction
          //   it is awful.
          Solver->setFailure("Failed to read dependencies");
          Log.error()
          << "Failed to read dependencies for '" << PackagePath << "'\n";
        }
      } else {
       Solver->setFailure("Failed to open one of dependency files");
       Log.error() << "Failed to open file '" << PackagePath << "'\n";
      }
    }
  }

  static void dump(llvm::raw_ostream &out, const ParsedDependencies &ParsedDependencies) {
    for (auto &PackageDependencies : ParsedDependencies) {
      DependenciesData &Data = *PackageDependencies.second;
      auto &Strings = *Data.Strings;
      out
      << "Package: " << *Strings.getItem(Data.PackageFilePathID) << "\n";

      if (
        Data.DeclarationDependencies.empty() &&
        Data.DefinitionDependencies.empty()
      ) {
        out
        << "    no dependencies.\n";
      } else {
        dump(out, 4, "Declaration depends on:", Strings, Data.DeclarationDependencies);
        dump(out, 4, "Definition depends on:", Strings, Data.DefinitionDependencies);
      }

      out << "\n";
    }
  }

  void buildDependenciesGraph() {
    if (!Solver->isValid())
      return;

    auto DGraph = DependenciesGraph::build(Context.getParsedDependencies());

    Context.DepsGraph = DGraph;

    if (DGraph->isInvalid()) {
      Log.error() << "Failed to solve dependencies. Unable to find root nodes.\n";
      if (!Solver->Verbose) {
        Log.error()
        << "Loaded dependencies:\n";
        dump(Log.error(), Context.getParsedDependencies());
      }
      return;
    }

    Log.verbose()
    << "Dependencies graph:\n";
    DGraph->dump(Log.verbose(), Context.StringsPool);
  }

  void solveGraph() {
    if (!Solver->isValid())
      return;

    std::shared_ptr<DependenciesGraph> DGraphPtr =
        Context.detachDependenciesGraph();

    // TODO Levitation: remove this way. Use singleton instead.
    auto &Strings = Context.getStringsPool();

      Log.verbose() << "Solving dependencies...\n";

      Context.SolvedDepsInfo = SolvedDependenciesInfo::build(DGraphPtr);

      const auto &SolvedInfo = *Context.SolvedDepsInfo;

      if (!SolvedInfo.isValid()) {
        Log.error() << "Failed to solve: " << SolvedInfo.getErrorMessage() << "\n";

        Log.error() << "Dependencies:\n";
        dump(Log.error(), Context.getParsedDependencies());

        Solver->setFailure("Failed to solve dependencies.");

        return;
      }

      Log.verbose()
      << "Dependencies solved info:\n";
      SolvedInfo.dump(Log.verbose(), Strings);
  }

  static void dump(
      llvm::raw_ostream &out,
      unsigned Indent,
      StringRef Title,
      DependenciesStringsPool &Strings,
      DependenciesData::DeclarationsBlock &Deps
  ) {
    if (Deps.empty())
      return;

    out.indent(Indent) << Title << "\n";
    for (auto &Dep : Deps) {
        out.indent(Indent + 4)
        << *Strings.getItem(Dep.FilePathID) << "\n";
    }
  }


  void writeResult() {
    if (!Solver->isValid())
      return;

    Log.verbose()
    << "Writing dependencies...\n";

    const auto &SolvedInfo = Context.getSolvedDependenciesInfo();

    if (!writeResult(SolvedInfo)) {
      Log.error()
      << "failed to write solved depenendices.\n";
      Log.error()
      << Solver->getErrorMessage() << "\n";
      return;
    }
  }

  using PathString = SinglePath;
  using DependenciesPaths = Paths;

  bool writeResult(
      const SolvedDependenciesInfo &SolvedDependencies
  ) {

    // TODO Levitation: remove this
    const auto &Strings = Context.getStringsPool();

    const DependenciesGraph &DGraph = SolvedDependencies.getDependenciesGraph();
    for (auto &NodeIt : SolvedDependencies.getDependenciesMap()) {
      auto NID = NodeIt.first;

      const auto &Node = DGraph.getNode(NID);

      StringRef ParentDir = Solver->BuildRoot;

      DependenciesPaths DirectDependencies = buildDirectDependencies(
          ParentDir, Strings, DGraph, Node.Dependencies
      );

      DependenciesPaths FullDependencies = buildFullDependencies(
          ParentDir, Strings, DGraph, NodeIt.second.FullDependencies
      );

      auto DependentFilePath = getNodeFilePath(
          ParentDir, Strings, Node
      );

      if (!writeDependenciesFile(
          *Solver,
          FileExtensions::DirectDependencies,
          DependentFilePath,
          DirectDependencies
      )) {
        return false;
      }

      if (!writeDependenciesFile(
          *Solver,
          FileExtensions::FullDependencies,
          DependentFilePath,
          FullDependencies
      )) {
        return false;
      }
    }

    return true;
  }

  PathString getNodeSourceFilePath(
      const StringRef ParentDir,
      const DependenciesStringsPool &Strings,
      const DependenciesGraph::Node &Node
  ) {
    PathString SourcePath = *Strings.getItem(Node.PackageInfo->PackagePath);

    StringRef NewExt = FileExtensions::SourceCode;

    updateFilePath(SourcePath, ParentDir, NewExt);

    return SourcePath;
  }

  PathString getNodeFilePath(
      const StringRef ParentDir,
      const DependenciesStringsPool &Strings,
      const DependenciesGraph::Node &Node
  ) {
    PathString SourcePath = *Strings.getItem(Node.PackageInfo->PackagePath);

    StringRef NewExt = Node.Kind == DependenciesGraph::NodeKind::Declaration ?
        FileExtensions::DeclarationAST :
        FileExtensions::Object;

    updateFilePath(SourcePath, ParentDir, NewExt);

    return SourcePath;
  }

  void updateFilePath(
      PathString &SourcePath,
      const StringRef ParentDir,
      StringRef NewExt
  ) {
    if (SourcePath.startswith("./"))
      SourcePath = SourcePath.substr(2);

    llvm::sys::path::replace_extension(SourcePath, NewExt);
    llvm::sys::fs::make_absolute(ParentDir, SourcePath);

    if (SourcePath.startswith("./"))
      SourcePath = SourcePath.substr(2);

    llvm::sys::fs::make_absolute(SourcePath);
  }

  DependenciesPaths buildDirectDependencies(
    StringRef DepsRoot,
    const DependenciesStringsPool &Strings,
    const DependenciesGraph &DGraph,
    const DependenciesGraph::NodesSet &Dependencies
  ) {
    DependenciesPaths Paths;
    for (auto NID : Dependencies) {
      auto Kind = DependenciesGraph::NodeID::getKind(NID);

      assert(
          Kind == DependenciesGraph::NodeKind::Declaration &&
          "Only declaration nodes are allowed to be dependencies"
      );

      const auto &N = DGraph.getNode(NID);

      // Also put source file into dependencies
      // Because we need to rebuild target in case
      // if source file deleted.
      auto NodeSourceFilePath = getNodeSourceFilePath(
          Context.Solver.SourcesRoot,
          Strings,
          N
      );
      Paths.push_back(NodeSourceFilePath);

      auto NodeFilePath = getNodeFilePath(
          DepsRoot, Strings, DGraph.getNode(NID)
      );
      Paths.push_back(std::move(NodeFilePath));
    }

    return Paths;
  }

  DependenciesPaths buildFullDependencies(
    StringRef DepsRoot,
    const DependenciesStringsPool &Strings,
    const DependenciesGraph &DGraph,
    const SolvedDependenciesInfo::RangedDependenciesMap &Dependencies
  ) {
    DependenciesPaths Paths;
    for (auto Dep : Dependencies) {

      auto &N = DGraph.getNode(Dep.second);

      PathString Package = *Strings.getItem(N.PackageInfo->PackagePath);

      assert(
          N.Kind == DependenciesGraph::NodeKind::Declaration &&
          "Only declaration nodes are allowed to be dependencies"
      );

      // Declaration AST is instantiated from parsed AST,
      // and thus latter depends on former.
      // The only exception is main file.
      DependenciesSolverPath::addDepPathsFor(
          Paths, Context.Solver.BuildRoot,
          Package,
          N.PackageInfo->IsMainFile
      );
    }

    return Paths;
  }

  bool writeDependenciesFile(
      DependenciesSolver &Solver,
      StringRef Extension,
      StringRef DependentFilePath,
      const DependenciesPaths &Dependencies
  ) {
    auto DependenciesFile = (DependentFilePath + "." + Extension).str();

    Log.verbose()
    << "Writing '" << DependenciesFile << "'...\n";

    File F(DependenciesFile);

    if (F.hasErrors()) {
      StringRef Error = "failed to write file '" + DependenciesFile + "'";
      Solver.setFailure(Error);
      return false;
    }

    with (auto Opened = F.open()) {
      auto &out = Opened.getOutputStream();

      for (auto D : Dependencies)
        out << D << "\n";
    }

    if (F.hasErrors()) {
      StringRef Error = "failed to complete file '" + DependenciesFile + "'";
      Solver.setFailure(Error);
      return false;
    }

    return true;
  }

  void solve() {
    collectParsedDependencies();
    buildDependenciesGraph();
    solveGraph();
  }
};

std::shared_ptr<SolvedDependenciesInfo>
DependenciesSolver::solve(const Paths &LDepsFiles) {
  auto &Log = log::Logger::get();

  DependenciesSolverContext Context(*this, LDepsFiles);
  DependenciesSolverImpl Impl(Context);

  Impl.solve();

  if (isValid()) {
    Log.verbose()
    << "\nComplete!\n";

    return Context.detachSolvedDependenciesInfo();
  }

  Log.error() << getErrorMessage() << "\n";
  return nullptr;
}

bool DependenciesSolver::solve() {

  log::Logger::createLogger(Verbose ? log::Level::Verbose : log::Level::Warning);
  CreatableSingleton<DependenciesStringsPool>::create();
  CreatableSingleton<FileManager>::create( FileSystemOptions { std::string(StringRef()) });

  Paths LDepsFiles;

  levitation::FileSystem::collectFiles(
      LDepsFiles,
      BuildRoot,
      FileExtensions::ParsedDependencies
  );

  auto &Log = log::Logger::get();

  DependenciesSolverContext Context(*this, LDepsFiles);
  DependenciesSolverImpl Impl(Context);

  Impl.solve();
  Impl.writeResult();

  if (isValid()) {
    Log.verbose()
    << "\nComplete!\n";

    return true;
  }

  Log.error() << getErrorMessage() << "\n";
  return false;
}

}}}

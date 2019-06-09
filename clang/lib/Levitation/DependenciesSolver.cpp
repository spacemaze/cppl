// TODO Levitation: Licensing

#include "clang/Levitation/Dependencies.h"
#include "clang/Levitation/DependenciesSolver.h"
#include "clang/Levitation/WithOperator.h"
#include "clang/Levitation/File.h"
#include "clang/Levitation/FileExtensions.h"
#include "clang/Levitation/Serialization.h"
#include "clang/Levitation/StringsPool.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/FileSystem.h"

#include "llvm/Support/raw_ostream.h"

#include <algorithm>

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

//=============================================================================
// Deserialized data object

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

  const DependenciesStringsPool &getStringsPool() const {
    return Strings;
  }
};

//=============================================================================
// Dependencies DAG

class DependenciesDAG {
public:
  struct Node;

  struct PackageInfo {
    StringID PackagePath;
    Node *Declaration;
    Node *Definition;
  };

  enum class NodeKind {
      Declaration,
      Definition
  };

  struct NodeID {
      using Type = uint64_t;

      static Type get(NodeKind Kind, StringID PathID) {

        const int NodeKindBits = 1;

        uint64_t PathIDWithoutHighestBits = (((uint64_t)~0) >> NodeKindBits) & PathID;

        return ((uint64_t)Kind << (64 - NodeKindBits) ) |
               PathIDWithoutHighestBits;
      }

      static std::pair<NodeKind, StringID> getKindAndPathID(Type ID) {
        const int NodeKindBits = 1;

        auto Kind = (NodeKind)(ID >> (64 - NodeKindBits));

        StringID PathIDWithoutHighestBits = (((uint64_t)~0) >> NodeKindBits) & ID;

        return { Kind, PathIDWithoutHighestBits };
      }

      static NodeKind getKind(Type ID) {
        NodeKind Kind;
        StringID PathID;

        std::tie(Kind, PathID) = getKindAndPathID(ID);

        return Kind;
      }
  };

  using NodesMap = DenseMap<NodeID::Type, std::unique_ptr<Node>>;
  using NodesSet = DenseSet<NodeID::Type>;
  using PackagesMap = DenseMap<StringID, std::unique_ptr<PackageInfo>>;

  struct Node {
    Node(NodeID::Type id, NodeKind kind)
    : ID(id), Kind(kind), PackageInfo(nullptr)
    {}
    NodeID::Type ID;
    NodeKind Kind;
    PackageInfo* PackageInfo;
    NodesSet Dependencies;
    NodesSet DependentNodes;
  };

private:

  /// Nodes whithout dependencies.
  NodesSet Roots;

  /// Declaration nodes without dependent declaration nodes.
  /// E.g. "B" depends on declaration of "A", wich means
  ///   that "B" definition depends on "B" declaration and
  ///   "B" declaration in turn depends on declaration of "A".
  ///   In this case "B" declaration is terminal declaration node,
  ///   even though it has dependent node (definition of "B").
  ///
  /// This tricky collection is required for .h files generation.
  /// For each declaration node we should generate .h file, but
  /// declaration terminal nodes provide us with hint of minimum
  /// possible .h files to be included in order to include
  /// everything.
  NodesSet DeclarationTerminals;

  NodesMap AllNodes;
  PackagesMap PackageInfos;

public:

  static std::unique_ptr<DependenciesDAG> build(
      ParsedDependencies &ParsedDeps,
      llvm::raw_ostream &out
  ) {
    auto DAGPtr = llvm::make_unique<DependenciesDAG>();

    with (auto A = DumpAction(out, "Building dependencies DAG")) {

      for (auto &PackageDeps : ParsedDeps) {

        auto PackagePathID = PackageDeps.first;
        DependenciesData &PackageDependencies = *PackageDeps.second;

        PackageInfo &Package = DAGPtr->createPackageInfo(PackagePathID);

        // If package declaration has no dependencies we add it into the Roots
        // collection.
        if (PackageDependencies.DeclarationDependencies.empty())
          DAGPtr->Roots.insert(Package.Declaration->ID);

        DAGPtr->addDependenciesTo(
            *Package.Declaration,
            PackageDependencies.DeclarationDependencies
        );

        DAGPtr->addDependenciesTo(
            *Package.Definition,
            PackageDependencies.DefinitionDependencies
        );
      }

      // Scan for declaration terminal nodes.
      DAGPtr->collectDeclarationTerminals();
    }

    return DAGPtr;
  }

  // TODO Levitation: That looks pretty much like A* walk.
  //
  void bsfWalkSkipVisited(std::function<void(const Node &)> OnNode) const {
    bsfWalk(true, OnNode);
  }

  void bsfWalk(std::function<void(const Node &)> OnNode) const {
    bsfWalk(false, OnNode);
  }

  // Do BFS graph walk and dump each node
  void dump(
      llvm::raw_ostream &out, const DependenciesStringsPool &Strings
  ) const {

    if (Roots.empty()) {
      out << "(empty)";
      return;
    }

    bsfWalkSkipVisited([&](const Node &N) {
        dumpNode(out, N.ID, Strings);
        out << "\n";
    });

    // If graph has cycles it may be possible that
    // we have non-empty graph and empty Terminals collection.
    // Don't fire error yet, do it later during dependency solve
    // stage.
    if (DeclarationTerminals.empty()) {
      out << "No terminal nodes found. Graph has cycles.\n";
      return;
    }

    out << "Declaration terminals:\n";
    for (auto TerminalNodeID : DeclarationTerminals) {
      out << "    ";
      dumpNodeID(out, TerminalNodeID);
      out << "\n";
    }
    out << "\n";
  }

  void dumpNode(
      llvm::raw_ostream &out,
      NodeID::Type NodeID,
      const DependenciesStringsPool &Strings
  ) const {

    const Node &Node = getNode(NodeID);

    out
    << "Node[";
    dumpNodeID(out, NodeID);
    out << "]\n"
    << "    Path: " << *Strings.getItem(Node.PackageInfo->PackagePath) << "\n"
    << "    Kind: "
    << (Node.Kind == NodeKind::Declaration ? "Declaration" : "Definition") << "\n";

    if (Node.DependentNodes.size()) {
      out << "    Is used by:\n";
      for (auto DependentNodeID : Node.DependentNodes) {
        out << "        ";
        dumpNodeID(out, DependentNodeID);
        out << "\n";
      }
    }

    if (Node.Dependencies.size()) {
      out << "    Dependencies:\n";
      for (auto DependencyID : Node.Dependencies) {
        out << "        ";
        dumpNodeID(out, DependencyID);
        out << "\n";
      }
    }
  }

  void dumpNodeID(llvm::raw_ostream &out, NodeID::Type NodeID) const {
    NodeKind Kind;
    StringID PathID;
    std::tie(Kind, PathID) = NodeID::getKindAndPathID(NodeID);

    out
    << PathID << ":"
    << (Kind == NodeKind::Declaration ? "DECL" : "DEF");
  }

  const Node &getNode(NodeID::Type ID) const {
    auto Found = AllNodes.find(ID);
    assert(
        Found != AllNodes.end() &&
        "Node with current ID should be present in AllNodes"
    );
    return *Found->second;
  }

  const NodesMap &allNodes() const { return AllNodes; }
  const NodesSet &roots() const { return Roots; }
  const NodesSet &declarationTerminals() const { return DeclarationTerminals; }

protected:

  void bsfWalk(
      bool SkipVisited,
      std::function<void(const Node&)> OnNode
  ) const {
    NodesSet Worklist = Roots;

    NodesSet VisitedNodes;
    NodesSet NewWorklist;

    while (Worklist.size()) {
      NewWorklist.clear();
      for (auto NID : Worklist) {

        if (SkipVisited && !VisitedNodes.insert(NID).second)
          continue;

        const auto &Node = getNode(NID);

        OnNode(Node);

        NewWorklist.insert(Node.DependentNodes.begin(), Node.DependentNodes.end());
      }
      Worklist.swap(NewWorklist);
    }
  }

  void addDependenciesTo(
      Node &DependentNode,
      const DependenciesData::DeclarationsBlock &Dependencies
  ) {
    auto DependentNodeID = DependentNode.ID;

    for (auto &Dep : Dependencies) {
      Node &DeclDependencyNode = getOrCreateNode(
          NodeKind::Declaration,
          Dep.FilePathID
      );

      DependentNode.Dependencies.insert(DeclDependencyNode.ID);
      DeclDependencyNode.DependentNodes.insert(DependentNodeID);
    }
  }

  PackageInfo &createPackageInfo(StringID PackagePathID) {

    auto PackageRes = PackageInfos.insert({
      PackagePathID, llvm::make_unique<PackageInfo>()
    });

    PackageInfo &Package = *PackageRes.first->second;

    assert(
        PackageRes.second &&
        "Only one package can be created for particular PackagePathID"
    );

    Node &DeclNode = getOrCreateNode(NodeKind::Declaration, PackagePathID);
    Node &DefNode = getOrCreateNode(NodeKind::Definition, PackagePathID);

    DeclNode.DependentNodes.insert(DefNode.ID);
    DefNode.Dependencies.insert(DeclNode.ID);

    DeclNode.PackageInfo = &Package;
    DefNode.PackageInfo = &Package;

    Package.PackagePath = PackagePathID;
    Package.Declaration = &DeclNode;
    Package.Definition = &DefNode;

    return Package;
  }

  Node &getOrCreateNode(
      NodeKind Kind, StringID PackagePathID
  ) {
    auto ID = NodeID::get(Kind, PackagePathID);
    auto InsertionRes = AllNodes.insert({ ID, nullptr });

    if (InsertionRes.second)
      InsertionRes.first->second.reset(new Node(ID, Kind));

    return *InsertionRes.first->second;
  }

  void collectDeclarationTerminals() {
    for (auto &NodeIt : AllNodes) {
      auto &N = *NodeIt.second;

      if (N.Kind != NodeKind::Declaration)
        continue;

      bool HasDependentDeclarationNodes = false;

      for (auto &DependentNodeID : N.DependentNodes) {
        NodeKind DependentNodeKind = NodeID::getKind(DependentNodeID);

        if (DependentNodeKind == NodeKind::Declaration) {
          HasDependentDeclarationNodes = true;
          break;
        }
      }

      if (!HasDependentDeclarationNodes)
        DeclarationTerminals.insert(NodeIt.first);
    }
  }
};

//=============================================================================
// Dependencies solving

class SolvedDependenciesInfo {

  using NodeID = DependenciesDAG::NodeID;
  using Node = DependenciesDAG::Node;

  struct DependencyWithDistance {
    NodeID::Type NodeID;
    int Range;
  };

  using FullDependencies =
      DenseMap<NodeID::Type, std::unique_ptr<DependencyWithDistance>>;

  using FullDependenciesMap =
      DenseMap<NodeID::Type, std::unique_ptr<FullDependencies>>;

  using FullDependenciesList = SmallVector<DependencyWithDistance, 16>;

  using FullDependenciesSortedMap =
      DenseMap<NodeID::Type, std::unique_ptr<FullDependenciesList>>;

  const DependenciesDAG &DDAG;
  FullDependenciesMap FullDepsMap;
  FullDependenciesSortedMap FullDepsSortedMap;

  SolvedDependenciesInfo(const DependenciesDAG &DDAG) : DDAG(DDAG) {}

public:


  static SolvedDependenciesInfo build(
      const DependenciesDAG &DDAG,
      const DependenciesStringsPool &Strings,
      llvm::raw_ostream &errs
  ) {

    SolvedDependenciesInfo SolvedInfo(DDAG);

    FullDependencies EmptyList;

    DDAG.bsfWalk([&] (const Node &N) {
      NodeID::Type NID = N.ID;

      const auto &CurrentDependencies = N.Dependencies.size() ?
          SolvedInfo.getDependencies(NID) :
          EmptyList;

      for (auto DependentNodeID : N.DependentNodes) {
        auto &NextDependencies =
            SolvedInfo.getOrCreateDependencies(DependentNodeID);

        SolvedInfo.merge(NextDependencies, CurrentDependencies, NID);
      }
    });

    SolvedInfo.sortAllDependencies();

    return SolvedInfo;
  }

  const FullDependencies &getDependencies(NodeID::Type NID) const {
    auto Found = FullDepsMap.find(NID);
    assert(
        Found != FullDepsMap.end() &&
        "Deps list should be present in FullDepsMap"
    );

    return *Found->second;
  }

  const FullDependenciesList &getDependenciesList(NodeID::Type NID) const {
    static FullDependenciesList EmptyList;

    auto Found = FullDepsSortedMap.find(NID);

    if (Found == FullDepsSortedMap.end())
      return EmptyList;

    return *Found->second;
  }

  void dump(
      llvm::raw_ostream &out,
      const DependenciesStringsPool &Strings
  ) const {
    DDAG.bsfWalkSkipVisited([&](const Node &N) {
      auto NID = N.ID;
      const FullDependenciesList &FullDeps = getDependenciesList(NID);

      const auto &Path = *Strings.getItem(N.PackageInfo->PackagePath);
      out << "[";
      DDAG.dumpNodeID(out, NID);
      out << "]\n";

      out << "    Path: " << Path << "\n";
      if (FullDeps.size()) {
        out
                << "    Full dependencies:\n";

        for (const auto &DepWithDist : FullDeps) {
          const auto &Dep = DDAG.getNode(DepWithDist.NodeID);
          const auto &DepPath = *Strings.getItem(Dep.PackageInfo->PackagePath);

          out.indent(8) << "[";
          DDAG.dumpNodeID(out, DepWithDist.NodeID);
          out
                  << "]: " << DepPath << "\n";
        }

        out
                << "    Direct dependencies:\n";

        for (const auto &DepID : N.Dependencies) {
          const auto &Dep = DDAG.getNode(DepID);
          const auto &DepPath = *Strings.getItem(Dep.PackageInfo->PackagePath);

          out
                  << "        " << DepPath << "\n";
        }
      } else {
        out << "    (root)\n";
      }
    });
  }

protected:

  FullDependencies &getOrCreateDependencies(NodeID::Type NID) {
    auto ListRes = FullDepsMap.insert({ NID, nullptr });

    if (ListRes.second)
      ListRes.first->second = llvm::make_unique<FullDependencies>();

    return *ListRes.first->second;
  }

  void merge(
      FullDependencies &DestList,
      const FullDependencies &PrevList,
      NodeID::Type DirectDependencyNodeID
  ) {
    for (const auto &DepWithDistIt : PrevList) {
      const auto &DepWithDist = *DepWithDistIt.second;

      insertOrUpdDepWithDistance(DestList, DepWithDist.NodeID, DepWithDist.Range + 1);
    }

    insertOrUpdDepWithDistance(DestList, DirectDependencyNodeID, 1);
  }

  void insertOrUpdDepWithDistance(
          FullDependencies &DestList,
          NodeID::Type NID,
          int Range
  ) {
      auto InsertionRes = DestList.insert( {NID, nullptr} );

      if (InsertionRes.second) {
        InsertionRes.first->second = llvm::make_unique<DependencyWithDistance>(
            DependencyWithDistance {NID, Range}
        );
      } else {
        auto &ExistingList = *InsertionRes.first->second;
        ExistingList.Range = std::max(ExistingList.Range, Range);
      }
  }

  void sortAllDependencies() {
    for (auto &NodeIt : FullDepsMap) {
      auto Res = FullDepsSortedMap.insert({
        NodeIt.first,
        llvm::make_unique<FullDependenciesList>()
      });

      FullDependencies &NodeDeps = *NodeIt.second;
      FullDependenciesList &List = *Res.first->second;
      sortNodeDependencies(List, NodeDeps);
    }
  }

  void sortNodeDependencies(
      FullDependenciesList &Dest, const FullDependencies &Src
  ) {
    for (const auto &DepWithDistIt : Src) {
      const DependencyWithDistance &Dep = *DepWithDistIt.second;
      Dest.push_back(Dep);
    }

    std::sort(
        Dest.begin(),
        Dest.end(),
        [=](const DependencyWithDistance &L, const DependencyWithDistance &R) {
            return L.Range > R.Range;
        }
    );
  }
};

using SolvedDependenciesMap = llvm::DenseMap<llvm::StringRef, std::unique_ptr<SolvedDependenciesInfo>>;

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
            // TODO Levitation: Do something with errors logging and DumpAction
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
      const SolvedDependenciesInfo &SolvedDependencies
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

  verbose()
  << "Loaded dependencies:\n";
  Helper.dump(parsedDependencies);

  auto DDAG = DependenciesDAG::build(parsedDependencies, verbose());

  verbose()
  << "Dependencies DAG:\n";
  DDAG->dump(verbose(), parsedDependencies.getStringsPool());

  auto SolvedInfo = SolvedDependenciesInfo::build(
      *DDAG,
      parsedDependencies.getStringsPool(),
      error()
  );

  verbose()
  << "Dependencies solved info:\n";
  SolvedInfo.dump(verbose(), parsedDependencies.getStringsPool());

  Helper.writeResult(SolvedInfo);
}

llvm::raw_ostream &DependenciesSolver::verbose() {
  return Verbose ? llvm::outs() : llvm::nulls();
}
llvm::raw_ostream &DependenciesSolver::error() {
  return llvm::errs();
}
}}

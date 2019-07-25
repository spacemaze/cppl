//===--------------------- DependenciesGraph.h ------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines bidirectional Dependencies Graph
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LEVITATION_DEPENDENCIES_GRAPH_H
#define LLVM_LEVITATION_DEPENDENCIES_GRAPH_H

#include "clang/Levitation/DependenciesSolver/ParsedDependencies.h"

#include "clang/Levitation/Common/WithOperator.h"
#include "clang/Levitation/Common/SimpleLogger.h"
#include "clang/Levitation/Common/StringsPool.h"
#include "clang/Levitation/TasksManager/TaskContext.h"
#include "clang/Levitation/TasksManager/TasksManager.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringRef.h"

#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <mutex>

namespace clang { namespace levitation { namespace dependencies_solver {

class DependenciesGraph {
public:
  struct Node;

  struct PackageInfo {
    StringID PackagePath;
    Node *Declaration;
    Node *Definition;
    bool IsMainFile = false;
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

  using NodesMap = llvm::DenseMap<NodeID::Type, std::unique_ptr<Node>>;
  using NodesSet = llvm::DenseSet<NodeID::Type>;
  using PackagesMap = llvm::DenseMap<StringID, std::unique_ptr<PackageInfo>>;

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

  bool Invalid = false;

public:

  static std::shared_ptr<DependenciesGraph> build(
      const ParsedDependencies &ParsedDeps,
      StringID MainFileID
  ) {
    auto DGraphPtr = std::make_shared<DependenciesGraph>();
    auto &Log = log::Logger::get();

    Log.verbose() << "Building dependencies graph...\n";

    for (auto &PackageDeps : ParsedDeps) {

      auto PackagePathID = PackageDeps.first;
      DependenciesData &PackageDependencies = *PackageDeps.second;

      PackageInfo &Package = DGraphPtr->createPackageInfo(PackagePathID);

      // If package declaration has no dependencies we add it into the Roots
      // collection.
      if (PackageDependencies.DeclarationDependencies.empty())
        DGraphPtr->Roots.insert(Package.Declaration->ID);

      DGraphPtr->addDependenciesTo(
          *Package.Declaration,
          PackageDependencies.DeclarationDependencies
      );

      DGraphPtr->addDependenciesTo(
          *Package.Definition,
          PackageDependencies.DefinitionDependencies
      );
    }

    if (!DGraphPtr->AllNodes.empty() && DGraphPtr->Roots.empty()) {
      DGraphPtr->Invalid = true;
    }


    // Create node which will represent main file
    PackageInfo &MainFilePackage = DGraphPtr->createMainFilePackage(MainFileID);

    // Scan for declaration terminal nodes.
    DGraphPtr->collectDeclarationTerminals(MainFilePackage);

    return DGraphPtr;
  }

  // TODO Levitation: That looks pretty much like A* walk.
  //
  void bsfWalkSkipVisited(std::function<void(const Node &)> &&OnNode) const {
    bsfWalk(true, [&] (const Node &N) { OnNode(N); return true; });
  }

  bool bsfWalk(std::function<bool(const Node &)> &&OnNode) const {
    return bsfWalk(false, std::move(OnNode));
  }

  /// Implements deep search first walk, and runs job on each node
  /// it meets.
  /// Starts from terminal nodes going down to roots.
  /// \param OnNode Action to be launched to process current node
  ///        but not its subnodes.
  /// \return true is walk was successful.
  bool dsfJobs(
      std::function<bool(const Node&)> &&OnNode
  ) const {
    JobsContext Jobs(std::move(OnNode));
    return dsfJobsOnNode(nullptr, DeclarationTerminals, Jobs);
  }

  bool isInvalid() const { return Invalid; }

  // Do BFS graph walk and dump each node
  void dump(
      llvm::raw_ostream &out, const DependenciesStringsPool &Strings
  ) const {

    if (Roots.empty()) {
      out << "(empty)\n\n";
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

  void dumpNodeShort(
      llvm::raw_ostream &out,
      NodeID::Type NodeID,
      const DependenciesStringsPool &Strings
  ) const {

    const Node &Node = getNode(NodeID);

    out
    << "Node[";
    dumpNodeID(out, NodeID);
    out << "]: " << *Strings.getItem(Node.PackageInfo->PackagePath);
  }


  static void dumpNodeID(llvm::raw_ostream &out, NodeID::Type NodeID) {
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

  using OnNodeFn = std::function<bool(const Node&)>;

  bool bsfWalk(
      bool SkipVisited,
      OnNodeFn &&OnNode
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

        if (!OnNode(Node))
          return false;

        NewWorklist.insert(Node.DependentNodes.begin(), Node.DependentNodes.end());
      }
      Worklist.swap(NewWorklist);
    }
    return true;
  }

  class JobsContext {
    llvm::DenseMap<NodeID::Type, tasks::TasksManager::TaskID> Tasks;
    OnNodeFn OnNode;
    std::mutex Mutex;

  public:

    JobsContext(OnNodeFn &&onNode) : OnNode(onNode) {}

    tasks::TasksManager::TaskID getJobForNode(
        NodeID::Type NID,
        tasks::TasksManager::ActionFn &&Fn
    ) {
      Mutex.lock();
      with (levitation::make_scope_exit([=] { Mutex.unlock(); } )) {

        auto Found = Tasks.find(NID);

        if (Found == Tasks.end()) {
          auto &TM = tasks::TasksManager::get();
          auto TID = TM.addTask(std::move(Fn));

          auto Res = Tasks.insert({NID, TID});
          assert(Res.second);

          return TID;
        }

        return Found->second;
      }
    }

    bool onNode(const Node &N) {
      return OnNode(N);
    }
  };

  bool dsfJobsOnNode(
      const Node *N,
      const NodesSet &SubNodes,
      JobsContext &Jobs
  ) const {

    bool Successful = false;

    if (SubNodes.size()) {

      tasks::TasksManager::TasksSet NodeTasks;

      // Note: Whe we can't process first subnode out of task manager?
      // Because that subnode may be used by another parent, and in order
      // not to run same job twise, we should assign unique TaskID for it,
      // and keep track of it in special Node-to-Task map.

      for (auto NID : SubNodes) {
        const auto &SubNode = getNode(NID);

        auto TID = Jobs.getJobForNode(
            SubNode.ID,
            [&](tasks::TaskContext &TC) {
              TC.Successful = dsfJobsOnNode(
                  &SubNode, SubNode.Dependencies, Jobs
              );
            }
        );

        NodeTasks.insert(TID);

        auto &TM = tasks::TasksManager::get();
        Successful = TM.waitForTasks(NodeTasks);
      }
    }

    if (Successful && N)
      Successful = Jobs.onNode(*N);

    return Successful;
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

  PackageInfo &createMainFilePackage(StringID MainFileID) {

    auto PackageRes = PackageInfos.insert({
      MainFileID, llvm::make_unique<PackageInfo>()
    });

    PackageInfo &Package = *PackageRes.first->second;

    assert(
        PackageRes.second &&
        "Only one package can be created for particular PackagePathID"
    );

    Node &DefNode = getOrCreateNode(NodeKind::Definition, MainFileID);

    DefNode.PackageInfo = &Package;

    Package.PackagePath = MainFileID;
    Package.Declaration = nullptr;
    Package.Definition = &DefNode;
    Package.IsMainFile = true;

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

  void collectDeclarationTerminals(PackageInfo &MainFilePackage) {
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

      if (!HasDependentDeclarationNodes) {
        auto &N = *NodeIt.second;
        auto &MainFileNode = *MainFilePackage.Definition;

        DeclarationTerminals.insert(N.ID);

        MainFileNode.Dependencies.insert(N.ID);
        N.DependentNodes.insert(MainFileNode.ID);
      }
    }
  }
};

}}} // end of clang::levitation::dependencies_solver namespace

#endif //LLVM_LEVITATION_DEPENDENCIES_GRAPH_H

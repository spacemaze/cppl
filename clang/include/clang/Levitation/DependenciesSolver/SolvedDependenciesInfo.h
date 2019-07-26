//===--------------------- SolvedDependenciesInfo.h ------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines bidirectional Solved Dependencies Info
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LEVITATION_SOLVED_DEPENDENCIES_INFO_H
#define LLVM_LEVITATION_SOLVED_DEPENDENCIES_INFO_H

#include "clang/Levitation/DependenciesSolver/DependenciesGraph.h"
#include "clang/Levitation/Common/Failable.h"
#include "clang/Levitation/Serialization.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringRef.h"

#include "llvm/Support/raw_ostream.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace clang { namespace levitation { namespace dependencies_solver {

class SolvedDependenciesInfo : public Failable {

  using NodeID = DependenciesGraph::NodeID;
  using Node = DependenciesGraph::Node;

  struct DependencyWithDistance {
    NodeID::Type NodeID;
    int Range;
  };

  using FullDependencies =
      llvm::DenseMap<NodeID::Type, std::unique_ptr<DependencyWithDistance>>;

  using FullDependenciesMap =
      llvm::DenseMap<NodeID::Type, std::unique_ptr<FullDependencies>>;
public:
  using FullDependenciesList = SmallVector<DependencyWithDistance, 16>;

  using FullDependenciesSortedMap =
      llvm::DenseMap<NodeID::Type, std::unique_ptr<FullDependenciesList>>;

private:
  std::shared_ptr<DependenciesGraph> DGraph;
  FullDependenciesMap FullDepsMap;
  FullDependenciesSortedMap FullDepsSortedMap;

  SolvedDependenciesInfo(
      std::shared_ptr<DependenciesGraph> &&DGraph
  )
  : DGraph(std::move(DGraph))
  {}

public:

  using OnDiagCyclesFoundFn = std::function<void(
      const FullDependenciesList &, NodeID::Type
  )>;

  static std::shared_ptr<SolvedDependenciesInfo> build(
      std::shared_ptr<DependenciesGraph> DGraphPtr,
      OnDiagCyclesFoundFn OnDiagCyclesFound
  ) {

    std::shared_ptr<SolvedDependenciesInfo> SolvedInfoPtr;

    SolvedInfoPtr.reset(new SolvedDependenciesInfo(
        std::move(DGraphPtr)
    ));

    SolvedDependenciesInfo &SolvedInfo = *SolvedInfoPtr;

    FullDependencies EmptyList;

    const auto &DGraph = SolvedInfoPtr->getDependenciesGraph();

    // TODO Levitation: if after BFS walk we have unvisited nodes,
    //   such unvisited nodes belong to isolated cycle islands,
    //   check those nodes and diagnose cycles.
    DependenciesGraph::NodesSet Unvisited;
    for (auto &NodeIt : DGraph.allNodes()) {
      Unvisited.insert(NodeIt.first);
    }

    bool Successfull =
      DGraph.bsfWalk([&] (const Node &N) {
        NodeID::Type NID = N.ID;

        const auto &CurrentDependencies = N.Dependencies.size() ?
            SolvedInfo.getDependencies(NID) :
            EmptyList;

        for (auto DependentNodeID : N.DependentNodes) {

          auto &NextDependencies =
              SolvedInfo.getOrCreateDependencies(DependentNodeID);

          SolvedInfo.merge(NextDependencies, CurrentDependencies, NID);

          // Diagnose cycles.
          if (NextDependencies.count(DependentNodeID)) {
            FullDependenciesList DepsList;
            sortNodeDependencies(DepsList, NextDependencies);
            OnDiagCyclesFound(DepsList, DependentNodeID);
            return false;
          }
        }

        Unvisited.erase(NID);
        return true;
      });

    if (!Successfull) {
      SolvedInfo.setFailure("Found cycles.");
      return SolvedInfoPtr;
    }

    if (Unvisited.size()) {
      SolvedInfo.setFailure("Found isolated cycles.");
      return SolvedInfoPtr;
    }

    // Put empty list for roots
    // Not sure we need it, but it will force dep-solver to
    // create empty dependency files.
    // And thus project build system will think everything is good.
    // For absence of dependency files looks like a trouble case.
    for (auto RootID : DGraph.roots()) {
      SolvedInfo.getOrCreateDependencies(RootID);
    }

    SolvedInfo.sortAllDependencies();

    return SolvedInfoPtr;
  }

  const FullDependencies &getDependencies(NodeID::Type NID) const {
    auto Found = FullDepsMap.find(NID);
    assert(
        Found != FullDepsMap.end() &&
        "Deps list should be present in FullDepsMap"
    );

    return *Found->second;
  }

  const DependenciesGraph &getDependenciesGraph() const {
    return *DGraph;
  }

  const FullDependenciesSortedMap &getDependenciesMap() const {
    return FullDepsSortedMap;
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

    auto &DGraphRef = *DGraph;

    DGraphRef.bsfWalkSkipVisited([&](const Node &N) {
      auto NID = N.ID;
      const FullDependenciesList &FullDeps = getDependenciesList(NID);

      const auto &Path = *Strings.getItem(N.PackageInfo->PackagePath);
      out << "[";
        DependenciesGraph::dumpNodeID(out, NID);
      out << "]\n";

      out << "    Path: " << Path << "\n";
      if (FullDeps.size()) {
        out
                << "    Full dependencies:\n";

        for (const auto &DepWithDist : FullDeps) {
          const auto &Dep = DGraphRef.getNode(DepWithDist.NodeID);
          const auto &DepPath = *Strings.getItem(Dep.PackageInfo->PackagePath);

          out.indent(8) << "[";
          DependenciesGraph::dumpNodeID(out, DepWithDist.NodeID);
          out
                  << "]: " << DepPath << "\n";
        }

        out
                << "    Direct dependencies:\n";

        for (const auto &DepID : N.Dependencies) {
          const auto &Dep = DGraphRef.getNode(DepID);
          const auto &DepPath = *Strings.getItem(Dep.PackageInfo->PackagePath);

          out
                  << "        " << DepPath << "\n";
        }
      } else {
        out << "    (root)\n";
      }
    });
  }

  static void dumpDependencies(
      llvm::raw_ostream &out,
      const DependenciesGraph &DGraph,
      const DependenciesStringsPool &Strings,
      const FullDependenciesList &Deps
  ) {
    if (Deps.empty()) {
      out << "(empty chain)";
      return;
    }

    size_t NumNodes = Deps.size();

    DGraph.dumpNodeShort(out, Deps[NumNodes-1].NodeID, Strings);

    for (size_t i = 1; i != NumNodes; ++i) {
      out << "\ndepends on: ";
      DGraph.dumpNodeShort(out, Deps[NumNodes-1-i].NodeID, Strings);
    }
    out << "\n\n";
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

  static void sortNodeDependencies(
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

}}} // end of clang::levitation::dependencies_solver namespace

#endif //LLVM_LEVITATION_SOLVED_DEPENDENCIES_INFO_H

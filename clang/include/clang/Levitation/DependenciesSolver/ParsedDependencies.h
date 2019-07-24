//===--------------------- ParsedDependencies.h -----------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines parsed dependencies data object
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LEVITATION_PARSEDDEPENDENCIES_H
#define LLVM_LEVITATION_PARSEDDEPENDENCIES_H

#include "clang/Levitation/Serialization.h"
#include "llvm/ADT/DenseMap.h"

namespace clang { namespace levitation { namespace dependencies_solver {

class ParsedDependencies {
  DependenciesStringsPool &Strings;

  using DependenciesMap = llvm::DenseMap<StringID, std::unique_ptr<DependenciesData>>;
  DependenciesMap Map;

public:

  ParsedDependencies(DependenciesStringsPool &Strings) : Strings(Strings) {}

  void add(const DependenciesData &Deps) {
    auto NewDeps = llvm::make_unique<DependenciesData>(&Strings);

    llvm::DenseMap<StringID, StringID> OldToNew;

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

}}} // end of clang::levitation::dependencies_solver namespace

#endif //LLVM_LEVITATION_PARSEDDEPENDENCIES_H

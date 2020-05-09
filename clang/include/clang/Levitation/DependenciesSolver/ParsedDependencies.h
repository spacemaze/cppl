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

    auto OldToNew = makeOldToNew(*Deps.Strings);

    auto LevitationPackage = createPackageFor(OldToNew[Deps.PackageFilePathID]);

    LevitationPackage->IsPublic = Deps.IsPublic;

    for (auto &DeclDep : Deps.DeclarationDependencies) {
      auto NewDepID = OldToNew[DeclDep.FilePathID];
      LevitationPackage->DeclarationDependencies.insert(Declaration(NewDepID));
    }

    for (auto &DeclDep : Deps.DefinitionDependencies) {
      auto NewDepID = OldToNew[DeclDep.FilePathID];
      LevitationPackage->DefinitionDependencies.insert(Declaration(NewDepID));
    }

    auto InsertionRes = Map.insert({
      LevitationPackage->PackageFilePathID,
      std::move(LevitationPackage)
    });

    assert(
        InsertionRes.second && "Loaded dependencies has been already added"
    );
  }

  DependenciesMap::const_iterator begin() const { return Map.begin(); }
  DependenciesMap::const_iterator end() const { return Map.end(); }

  // TODO Levitation: Deprecated
  const DependenciesStringsPool &getStringsPool() const {
    return Strings;
  }

private:

  llvm::DenseMap<StringID, StringID> makeOldToNew(const DependenciesStringsPool& OldStrings) {

    llvm::DenseMap<StringID, StringID> OldToNew;

    for (auto &OldS : OldStrings.items()) {
      auto NewID = Strings.addItem(OldS.second);
      OldToNew[OldS.first] = NewID;
    }

    return OldToNew;
  }

  std::unique_ptr<DependenciesData> createPackageFor(
      StringID NewPackageID,
      std::function<bool(PathsPoolTy::key_type)>&& addIfPredicate = nullptr
  ) {
    if (addIfPredicate == nullptr || addIfPredicate(NewPackageID))
      return std::make_unique<DependenciesData>(&Strings, NewPackageID);

    return nullptr;
  }

  std::unique_ptr<DependenciesData> createPackageFor(
      const DependenciesStringsPool& OldStrings,
      StringID OldPackageID,
      std::function<bool(PathsPoolTy::key_type)>&& addIfPredicate = nullptr
  ) {
    auto OldToNew = makeOldToNew(OldStrings);

    auto NewPackageID = OldToNew[OldPackageID];

    if (addIfPredicate == nullptr || addIfPredicate(NewPackageID))
      return std::make_unique<DependenciesData>(&Strings, NewPackageID);

    return nullptr;
  }

  DependenciesData& getOrCreatePackageFor(
      const DependenciesStringsPool& OldStrings,
      StringID OldPackageID,
      bool *Created = nullptr
  ) {

    std::pair<DependenciesMap::iterator, bool> InsertionRes;

    auto PackagePtr = createPackageFor(
        OldStrings,
        OldPackageID,
        [&] (PathsPoolTy::key_type NewPackageID) {
          InsertionRes = Map.insert({ NewPackageID, nullptr });
          return InsertionRes.second;
        }
    );

    if (InsertionRes.second) {
      assert(PackagePtr);
      PackagePtr->IsExternal = true;
      InsertionRes.first->second = std::move(PackagePtr);
    }

    if (Created)
      *Created = InsertionRes.second;

    return *InsertionRes.first->second;
  }

  DependenciesData& getOrCreatePackageFor(
      StringID NewPackageID,
      bool *Created = nullptr
  ) {

    std::pair<DependenciesMap::iterator, bool> InsertionRes;

    auto PackagePtr = createPackageFor(
        NewPackageID,
        [&] (PathsPoolTy::key_type NewPackageID) {
          InsertionRes = Map.insert({ NewPackageID, nullptr });
          return InsertionRes.second;
        }
    );

    if (InsertionRes.second) {
      assert(PackagePtr);
      PackagePtr->IsExternal = true;
      InsertionRes.first->second = std::move(PackagePtr);
    }

    if (Created)
      *Created = InsertionRes.second;

    return *InsertionRes.first->second;
  }

};

}}} // end of clang::levitation::dependencies_solver namespace

#endif //LLVM_LEVITATION_PARSEDDEPENDENCIES_H

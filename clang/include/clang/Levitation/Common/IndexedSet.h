//===--- C++ Levitation IndexedSet.h ------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines C++ Levitation IndexedSet.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_INDEXEDSET_H
#define LLVM_CLANG_LEVITATION_INDEXEDSET_H

#include "llvm/ADT/DenseMap.h"

#include <map>

namespace clang { namespace levitation {

  /// IndexedSet is a semantic extension for regular sets concept.
  /// In addition to regular set it adds association of each
  /// set item with unique identifier number. And thus allows to access
  /// items by IDs rather than set operators.
  ///
  /// The advantage is that ID type is known to IndexedSet user and thus
  /// it is easier to serialize it and perform some other manipulations.
  ///
  /// \tparam IdTy type of identifier number, it should be integer
  ///     or at least mimic integer type behaviour
  ///    (by means of overloaded operators and so on).
  /// \tparam ItemTy type of items to be stored in set.
  ///
  /// \tparam ItemRefTy type of item reference, if provided allows
  ///    to reduce memory required by index collection.
  template <typename IdTy, typename ItemTy, typename ItemRefTy = ItemTy>
  class IndexedSet {

      // TODO Levitation: so far, I give up and use std::map.
      typedef typename std::map<ItemTy, IdTy> SetTy;
      typedef typename SetTy::iterator set_iterator;

      typedef typename llvm::DenseMap<IdTy, ItemTy> IndexTy;
      typedef typename IndexTy::const_iterator const_iterator;


      SetTy Set;
      IndexTy Index;

      IdTy LastIndex;

      static IdTy getInvalidIndex() {
        static IdTy Invalid = IdTy();
        return Invalid;
      }

  public:

      IndexedSet() : LastIndex(getInvalidIndex()) {}

      llvm::iterator_range<const_iterator> items() const {
        return llvm::iterator_range<const_iterator>(Index.begin(), Index.end());
      }

      bool addItem(IdTy Id, const ItemTy& Item) {
        auto Res = Set.emplace(Item, Id);

        if (!Res.second)
          return false;

        setIndex(Id, Item);

        return true;
      }

      IdTy addItem(ItemTy&& Item) {
        auto Res = Set.emplace(std::move(Item), getInvalidIndex());

        if (Res.second)
          return addIndex(Res.first);

        return Res.first->second;
      }

      IdTy addItem(const ItemTy& Item) {
        auto Res = Set.insert({ Item, getInvalidIndex() });

        if (Res.second)
          return addIndex(Res.first);

        return Res.first->second;
      }

      const ItemTy* getItem(const IdTy &Id) const {
        const auto Found = Index.find(Id);
        if (Found != Index.end())
          return &Found->second;

        return nullptr;
      }

  private:

      IdTy addIndex(set_iterator &SetIt) {
        IdTy NewIndex = LastIndex + 1;

        SetIt->second = NewIndex;
        Index.insert({ NewIndex, ItemRefTy(SetIt->first) });

        LastIndex = NewIndex;
        return LastIndex;
      }

      void setIndex(IdTy NewIndex, const ItemTy &Item) {
        auto Res = Index.insert({ NewIndex, ItemRefTy(Item) });
        assert(Res.second && "Index should be new");

        if (LastIndex <= NewIndex)
          LastIndex = NewIndex + 1;
      }
  };

}}

#endif //LLVM_CLANG_LEVITATION_INDEXEDSET_H

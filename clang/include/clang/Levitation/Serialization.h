// TODO Levitation: Licensing

#ifndef LLVM_CLANG_LEVITATION_SERIALIZATION_H
#define LLVM_CLANG_LEVITATION_SERIALIZATION_H

#include "clang/Basic/SourceLocation.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Bitcode/BitCodes.h"
#include <map>
#include <memory>

namespace llvm {
    class raw_ostream;
}

namespace clang {
namespace levitation {

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

      IdTy addItem(ItemTy&& Item) {
        auto Res = Set.emplace(std::move(Item), getInvalidIndex());

        if (Res.second)
          return addIndex(Res.first, Item);

        return Res.first->second;
      }

      IdTy addItem(const ItemTy& Item) {
        auto Res = Set.insert({ Item, getInvalidIndex() });

        if (Res.second)
          return addIndex(Res.first, Item);

        return Res.first->second;
      }

      const ItemTy* getItem(const IdTy &Id) {
        const auto Found = Index.find(Id);
        if (Found != Index.end())
          return &Found->second;

        return nullptr;
      }

  private:

      IdTy addIndex(set_iterator &SetIt, const ItemTy &Item) {
        IdTy NewIndex = LastIndex + 1;

        SetIt->second = NewIndex;
        Index.insert({ NewIndex, ItemRefTy(Item) });

        LastIndex = NewIndex;
        return LastIndex;
      }
  };

  struct DependenciesData {

    /// String identifier type in strings table.
    typedef uint32_t StringIDType;

    /// Strings table. As strings we store file paths and components.
    typedef IndexedSet<StringIDType, StringRef> StringsTable;

    typedef uint32_t LocationIDType;

    struct Declaration {
      StringIDType FilePathID;
      LocationIDType LocationIDBegin;
      LocationIDType LocationIDEnd;
    };

    typedef SmallVector<Declaration, 32> DeclarationsBlock;

    StringsTable Strings;

    StringIDType DependentUnitFilePathID;

    DeclarationsBlock DeclarationDependencies;
    DeclarationsBlock DefinitionDependencies;
  };

  struct ValidatedDependencies;
  class DependenciesWriter {
  public:
    virtual ~DependenciesWriter() = default;
    virtual void writeAndFinalize(ValidatedDependencies &Dependencies) = 0;
  };

  enum DependenciesRecordTypes {
      DEPS_DECLARATION_RECORD_ID = 1,
      DEPS_UNIT_FILE_PATH_RECORD_ID,
      DEPS_STRING_RECORD_ID,
  };

  /// Describes the various kinds of blocks that occur within
  /// an Dependencies file.
  enum DependenciesBlockIDs {
    DEPS_STRINGS_BLOCK_ID = llvm::bitc::FIRST_APPLICATION_BLOCKID,
    DEPS_DEPENDENCIES_MAIN_BLOCK_ID,
    DEPS_DECLARATION_DEPENDENCIES_BLOCK_ID,
    DEPS_DEFINITION_DEPENDENCIES_BLOCK_ID,
  };

  std::unique_ptr<DependenciesWriter> CreateBitstreamWriter(llvm::raw_ostream &OS);
}
}

#endif
//===--- C++ Levitation Serialization.h -------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines C++ Levitation public serialization data classes.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_SERIALIZATION_H
#define LLVM_CLANG_LEVITATION_SERIALIZATION_H

#include "clang/Basic/SourceLocation.h"
#include "clang/Levitation/Common/IndexedSet.h"
#include "clang/Levitation/Common/Path.h"
#include "clang/Levitation/Common/StringsPool.h"
#include "clang/Levitation/DeclASTMeta/DeclASTMeta.h"
#include "llvm/Bitstream/BitCodes.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include <map>
#include <memory>

namespace llvm {
    class raw_ostream;
    class MemoryBuffer;
}

namespace clang {
namespace levitation {
  struct Declaration {
    explicit Declaration(PathsPoolTy::key_type unitIdentifier)
    : UnitIdentifier(unitIdentifier) {}

    // So far, only one field
    StringID UnitIdentifier;

    // TODO Levitation: add location info
  };
}
}

namespace llvm {
template<>
struct DenseMapInfo<clang::levitation::Declaration> {
  static inline clang::levitation::Declaration getEmptyKey() {
    return clang::levitation::Declaration{(unsigned int) (-1)};
  }

  static inline clang::levitation::Declaration getTombstoneKey() {
    return clang::levitation::Declaration{(unsigned int) (-2)};
  }

  static unsigned getHashValue(const clang::levitation::Declaration &Val) {
    return (unsigned) (Val.UnitIdentifier * 37U);
  }

  static bool isEqual(
      const clang::levitation::Declaration &LHS,
      const clang::levitation::Declaration &RHS
  ) {
    return LHS.UnitIdentifier == RHS.UnitIdentifier;
  }
};

}

namespace clang {
namespace levitation {

  using DependenciesStringsPool = PathsPoolTy;


  struct DependenciesData {

    typedef DenseSet<Declaration> DeclarationsBlock;

    std::unique_ptr<DependenciesStringsPool> Strings;
    bool OwnStringsPool = true;

    DeclarationsBlock DeclarationDependencies;
    DeclarationsBlock DefinitionDependencies;

    /// Whether this file should publish its interface
    bool IsPublic;

    /// Whether we don't have declaration part
    bool IsBodyOnly;

    DependenciesData()
    : Strings(new DependenciesStringsPool),
      OwnStringsPool(true)
    {}

    DependenciesData(DependenciesStringsPool *Strings)
    : Strings(Strings),
      OwnStringsPool(false)
    {}

    DependenciesData(DependenciesData &&Src)
    : Strings(std::move(Src.Strings)),
      DeclarationDependencies(std::move(Src.DeclarationDependencies)),
      DefinitionDependencies(std::move(Src.DefinitionDependencies))
    {}

    ~DependenciesData() {
      if (!OwnStringsPool)
        Strings.release();
    }
  };

  struct PackageDependencies;
  class DependenciesWriter {
  public:
    virtual ~DependenciesWriter() = default;
    virtual void writeAndFinalize(PackageDependencies &Dependencies) = 0;
  };

  class Failable;
  class DependenciesReader {
  public:
    virtual ~DependenciesReader() = default;
    virtual bool read(DependenciesData &Dependencies) = 0;
    virtual const Failable &getStatus() const = 0;
  };

  class DeclASTMetaWriter {
  public:
    virtual ~DeclASTMetaWriter() = default;
    virtual void writeAndFinalize(const DeclASTMeta &Meta) = 0;
  };

  class DeclASTMetaReader {
  public:
    virtual ~DeclASTMetaReader() = default;
    virtual bool read(DeclASTMeta &Meta) = 0;
    virtual const Failable &getStatus() const = 0;
  };

  enum DependenciesRecordTypes {
      DEPS_INVALID_RECORD_ID = 0,
      DEPS_DECLARATION_RECORD_ID = 1,
      DEPS_PACKAGE_TOP_LEVEL_FIELDS_RECORD_ID,
      DEPS_STRING_RECORD_ID,
      DEPS_IDS_SET_RECORD_ID,
  };

  enum CommonBlockIDs {
    INVALID_BLOCK_ID = llvm::bitc::FIRST_APPLICATION_BLOCKID,
    FIRST_VALID_BLOCK_ID = llvm::bitc::FIRST_APPLICATION_BLOCKID + 1
  };

  /// Describes the various kinds of blocks that occur within
  /// an Dependencies file.
  enum DependenciesBlockIDs {
    DEPS_STRINGS_BLOCK_ID = FIRST_VALID_BLOCK_ID,
    DEPS_DEPENDENCIES_MAIN_BLOCK_ID,
    DEPS_DECLARATION_DEPENDENCIES_BLOCK_ID,
    DEPS_DEFINITION_DEPENDENCIES_BLOCK_ID,
  };

  enum MetaRecordTypes {
    META_INVALID_RECORD_ID = 0,
    META_TOP_LEVEL_FIELDS_RECORD_ID = 1,
    META_SOURCE_HASH_RECORD_ID,
    META_DECL_AST_HASH_RECORD_ID,
    META_SKIPPED_FRAGMENT_RECORD_ID
  };

  /// Describes the various kinds of blocks that occur within
  /// an Dependencies file.
  enum MetaBlockIDs {
    META_ARRAYS_BLOCK_ID = FIRST_VALID_BLOCK_ID,
    META_SKIPPED_FRAGMENT_BLOCK_ID
  };

  std::unique_ptr<DependenciesWriter> CreateBitstreamWriter(llvm::raw_ostream &OS);
  std::unique_ptr<DependenciesReader> CreateBitstreamReader(const llvm::MemoryBuffer &MemBuf);

  std::unique_ptr<DeclASTMetaWriter> CreateMetaBitstreamWriter(llvm::raw_ostream &OS);
  std::unique_ptr<DeclASTMetaReader> CreateMetaBitstreamReader(const llvm::MemoryBuffer &MemBuf);

}
}

#endif
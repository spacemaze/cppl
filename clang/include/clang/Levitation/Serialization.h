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
#include "clang/Levitation/IndexedSet.h"
#include "clang/Levitation/StringsPool.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Bitcode/BitCodes.h"
#include <map>
#include <memory>

namespace llvm {
    class raw_ostream;
    class MemoryBuffer;
}

namespace clang {
namespace levitation {

  using DependenciesStringsPool = StringsPool<256>;

  struct DependenciesData {

    typedef uint32_t LocationIDType;

    struct Declaration {
      StringID FilePathID;
      LocationIDType LocationIDBegin;
      LocationIDType LocationIDEnd;
    };

    typedef SmallVector<Declaration, 32> DeclarationsBlock;

    std::unique_ptr<DependenciesStringsPool> Strings;
    bool OwnStringsPool = true;

    StringID PackageFilePathID;

    DeclarationsBlock DeclarationDependencies;
    DeclarationsBlock DefinitionDependencies;

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

  class DependenciesReader {
  public:
    virtual ~DependenciesReader() = default;
    virtual bool read(DependenciesData &Dependencies) = 0;
    virtual StringRef getErrorMessage() const = 0;
  };

  enum DependenciesRecordTypes {
      DEPS_INVALID_RECORD_ID = 0,
      DEPS_DECLARATION_RECORD_ID = 1,
      DEPS_PACKAGE_FILE_PATH_RECORD_ID,
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
  std::unique_ptr<DependenciesReader> CreateBitstreamReader(const llvm::MemoryBuffer &MemBuf);
}
}

#endif
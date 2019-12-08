//===--- DeclASTMeta.h - C++ DeclASTMeta class ------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file contains root clas for Decl AST meta data
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LEVITATION_DECLASTMETA_H
#define LLVM_LEVITATION_DECLASTMETA_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"

#include "clang/Levitation/Common/Utility.h"
#include "clang/Levitation/Common/Path.h"

#include <utility>

namespace clang { namespace levitation {
  class DeclASTMeta {
  public:

    struct FragmentTy {
      size_t Start, End;
      bool ReplaceWithSemicolon;
      bool size() const { return End - Start; }
    };
    typedef SmallVector<FragmentTy, 64> RangesVec;

  private:

    llvm::SmallVector<uint8_t, 16> SourceHash;
    llvm::SmallVector<uint8_t, 16> DeclASTHash;
    RangesVec SkippedBytes;

  public:

    DeclASTMeta() = default;

    DeclASTMeta(
        llvm::ArrayRef<uint8_t> sourceHash,
        llvm::ArrayRef<uint8_t> declASTHash,
        const RangesVec &skippedBytes
    )
    : SourceHash(sourceHash.begin(), sourceHash.end()),
      DeclASTHash(declASTHash.begin(), declASTHash.end()),
      SkippedBytes(skippedBytes) {}

    const RangesVec &getSkippedBytes() const {
      return SkippedBytes;
    }

    const llvm::SmallVector<uint8_t, 16> &getSourceHash() const {
      return SourceHash;
    }

    const llvm::SmallVector<uint8_t, 16> &getDeclASTHash() const {
      return DeclASTHash;
    }

    void addSkippedFragment(size_t Start, size_t End, bool replaceWithSemicolon) {
      SkippedBytes.push_back({Start, End, replaceWithSemicolon});
    }

    template <typename RecordTy>
    void setSourceHash(const RecordTy &Record) {
      SourceHash.insert(SourceHash.begin(), Record.begin(), Record.end());
    }

    template <typename RecordTy>
    void setDeclASTHash(const RecordTy &Record) {
      DeclASTHash.insert(DeclASTHash.begin(), Record.begin(), Record.end());
    }
  };
}}

#endif //LLVM_LEVITATION_DECLASTMETA_H

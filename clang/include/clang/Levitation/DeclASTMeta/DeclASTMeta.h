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
    llvm::SmallVector<uint8_t, 16> SourceHash;
    llvm::SmallVector<uint8_t, 16> DeclASTHash;
    RangesVector SkippedBytes;
  public:

    DeclASTMeta() = default;

    DeclASTMeta(
        llvm::ArrayRef<uint8_t> sourceHash,
        llvm::ArrayRef<uint8_t> declASTHash,
        const RangesVector &skippedBytes
    )
    : SourceHash(sourceHash.begin(), sourceHash.end()),
      DeclASTHash(declASTHash.begin(), declASTHash.end()),
      SkippedBytes(skippedBytes) {}

    const RangesVector &getSkippedBytes() const {
      return SkippedBytes;
    }

    const llvm::SmallVector<uint8_t, 16> &getSourceHash() const {
      return SourceHash;
    }

    const llvm::SmallVector<uint8_t, 16> &getDeclASTHash() const {
      return DeclASTHash;
    }
  };
}}

#endif //LLVM_LEVITATION_DECLASTMETA_H

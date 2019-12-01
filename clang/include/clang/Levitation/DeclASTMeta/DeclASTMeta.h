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

#include "llvm/ADT/SmallVector.h"

#include "clang/Levitation/Common/Utility.h"
#include "clang/Levitation/Common/Path.h"

#include <utility>

namespace clang { namespace levitation { namespace tools {
 class DeclASTMeta {
   RangesVector SkippedBytes;

   // FIXME Levitation: to be implemented
   // llvm::SmallVector<uint8_t, 16> SourceHash;
   // llvm::SmallVector<uint8_t, 16> DeclASTHash;
 public:
   const RangesVector &getSkippedBytes() const {
     return SkippedBytes;
   }

   const llvm::SmallVector<uint8_t, 16> &getSourceHash() const {
     llvm_unreachable("not impelemented");
   }

   const llvm::SmallVector<uint8_t, 16> &getDeclASTHash() const {
     llvm_unreachable("not impelemented");
   }

   static DeclASTMeta loadFromFile(const SinglePath &Path) {
     DeclASTMeta Res;

     llvm_unreachable("not implemented");
     return Res;
   }
 };
}}}

#endif //LLVM_LEVITATION_DECLASTMETA_H

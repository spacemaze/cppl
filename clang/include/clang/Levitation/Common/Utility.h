//===--- C++ Levitation Utility.h -------------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines C++ Levitation utility types
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_UTILITY_H
#define LLVM_CLANG_LEVITATION_UTILITY_H

#include "clang/Basic/FileManager.h"
#include "clang/Levitation/Common/StringBuilder.h"
#include "clang/Levitation/Common/CreatableSingleton.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MD5.h"


#include <algorithm>
#include <utility>

namespace clang { namespace levitation {

typedef llvm::ArrayRef<uint8_t> HashRef;
typedef llvm::SmallVector<uint8_t, 16> HashVectorTy;

typedef std::pair<size_t, size_t> RangeTy;
typedef llvm::SmallVector<RangeTy, 64> RangesVector;

template<typename BuffT>
static inline llvm::MD5::MD5Result calcMD5(BuffT Buff) {
  llvm::MD5 Md5Builder;
  Md5Builder.update(Buff);
  llvm::MD5::MD5Result Result;
  Md5Builder.final(Result);
  return Result;
}

static inline bool calcMD5FromFile(
    FileManager &FM, llvm::MD5::MD5Result &Res, llvm::StringRef FileName
) {
  if (auto Buffer = FM.getBufferForFile(FileName)) {
    Res = calcMD5(Buffer.get()->getBuffer());
    return true;
  } else
    return false;
}


template <typename ArrLeftT, typename ArrRightT>
bool equal(const ArrLeftT &L, const ArrRightT &R) {
  return std::equal(L.begin(), L.end(), R.begin(), R.end());
}

}}

#endif //LLVM_CLANG_LEVITATION_UTILITY_H

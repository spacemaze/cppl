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

#include "clang/Levitation/Common/StringBuilder.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

namespace clang { namespace levitation {

typedef std::pair<size_t, size_t> RangeTy;
typedef llvm::SmallVector<RangeTy, 64> RangesVector;

}}

#endif //LLVM_CLANG_LEVITATION_UTILITY_H

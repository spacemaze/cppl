//===--- C++ Levitation FileExtensions.h ------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines standard C++ Levitation StringsPool class,
//  which is basically specialization of levitation::IndexedSet class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_STRINGSPOOL_H
#define LLVM_CLANG_LEVITATION_STRINGSPOOL_H

#include "clang/Levitation/IndexedSet.h"
#include "llvm/ADT/SmallString.h"

namespace clang { namespace levitation {

using StringID = uint32_t;

template<unsigned N>
using StringsPool = IndexedSet<StringID, llvm::SmallString<N> >;

}}

#endif //LLVM_STRINGSPOOL_H

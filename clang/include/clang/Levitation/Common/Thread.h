//===--- C++ Levitation Utility.h -------------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines C++ Levitation thread helpers
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_THREAD_H
#define LLVM_CLANG_LEVITATION_THREAD_H

#include <mutex>

namespace clang { namespace levitation {

using MutexLock = std::unique_lock<std::mutex>;
inline MutexLock lock(std::mutex &M) {
  return MutexLock(M);
}

}}

#endif //LLVM_CLANG_LEVITATION_THREAD_H

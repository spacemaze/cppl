//===--- C++ Levitation TaskContext.h ------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines C++ Levitation TaskContext class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_TASKCONTEXT_H
#define LLVM_CLANG_LEVITATION_TASKCONTEXT_H

namespace clang { namespace levitation {
  class Failable;
}}

namespace clang { namespace levitation { namespace tasks {

struct TaskContext {
  bool Successful = true;
};

}}}

#endif //LLVM_CLANG_LEVITATION_TASKCONTEXT_H

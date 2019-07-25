//===--- C++ Levitation Task.h ------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines C++ Levitation Task class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_TASK_H
#define LLVM_CLANG_LEVITATION_TASK_H

#include <functional>
#include <utility>

namespace clang { namespace levitation { namespace tasks {

struct TaskContext;

class Task {
public:
  using ActionFn = std::function<void(TaskContext&)>;

private:
  ActionFn Action;
public:
  /*implicit*/
  Task(ActionFn &&action) : Action(std::move(action)) {}
};

}}}

#endif //LLVM_CLANG_LEVITATION_TASK_H

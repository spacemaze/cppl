//===--- C++ Levitation TasksManager.h ------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines C++ Levitation TasksManager class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_TASKSMANAGER_H
#define LLVM_CLANG_LEVITATION_TASKSMANAGER_H

#include "clang/Levitation/Common/CreatableSingleton.h"
#include "clang/Levitation/Common/Failable.h"

#include "llvm/ADT/DenseSet.h"

namespace clang { namespace levitation { namespace tasks {

struct TaskContext;

class TasksManager : public CreatableSingleton<TasksManager> {
public:
  using TaskID = int;
  using ActionFn = std::function<void(TaskContext&)>;
  using TasksSet = llvm::DenseSet<TaskID>;

private:
  int JobsNumber;
protected:

  TasksManager(int jobsNumber)
  : JobsNumber(jobsNumber)
  {}

  friend CreatableSingleton<TasksManager>;

public:

  TaskID addTask(ActionFn &&Fn);
  bool executeTask(ActionFn &&Fn);
  bool waitForTasks();
  bool waitForTasks(const TasksSet &tasksSet);
};

}}}

#endif //LLVM_CLANG_LEVITATION_TASKSMANAGER_H

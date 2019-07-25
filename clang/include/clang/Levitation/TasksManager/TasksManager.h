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
#include "clang/Levitation/TasksManager/Task.h"

#include "llvm/ADT/DenseSet.h"

namespace clang { namespace levitation { namespace tasks {

class TasksManager : public CreatableSingleton<TasksManager> {
  int JobsNumber;
protected:

  TasksManager(int jobsNumber)
  : JobsNumber(jobsNumber)
  {}

  friend CreatableSingleton<TasksManager>;

public:

  void addTask(const Task &Task);
  Failable executeTask(const Task &Task);
  Failable waitForTasks();

  using TasksSet = llvm::DenseSet<const Task&>;
  Failable waitForTasks(const TasksSet &tasksSet);
};

}}}

#endif //LLVM_CLANG_LEVITATION_TASKSMANAGER_H

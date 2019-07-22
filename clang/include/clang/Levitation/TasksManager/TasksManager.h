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
#include "clang/Levitation/TasksManager/Task.h"


namespace clang { namespace levitation { namespace tasks {

class TasksManager : public CreatableSingleton<TasksManager> {
  int JobsNumber;
protected:

  TasksManager(int jobsNumber)
  : JobsNumber(jobsNumber)
  {}

  friend CreatableSingleton<TasksManager>;

public:

  void addTask(Task &&Task);
  void executeTask(Task &&Task);
  void waitForTasks();
};

}}}

#endif //LLVM_CLANG_LEVITATION_TASKSMANAGER_H

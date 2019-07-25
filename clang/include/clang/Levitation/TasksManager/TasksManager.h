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
#include "clang/Levitation/Common/WithOperator.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"

#include <memory>
#include <mutex>

namespace clang { namespace levitation { namespace tasks {

class TasksManager : public CreatableSingleton<TasksManager> {
public:
  using TaskID = int;

  struct TaskContext {
    TaskContext(TaskID tid) : ID(tid) {}
    TaskID ID;
    bool Successful = true;
  };
  using ActionFn = std::function<void(TaskContext&)>;
  using TasksSet = llvm::DenseSet<TaskID>;

private:
  int JobsNumber;

  std::mutex TaskStatusesAccess;
  int NextTaskID = 0;

  llvm::DenseMap<TaskID, std::unique_ptr<TaskContext>> TasksStatus;

protected:

  TasksManager(int jobsNumber)
  : JobsNumber(jobsNumber)
  {}

  friend CreatableSingleton<TasksManager>;

public:

  TaskID addTask(ActionFn &&Fn) {
    // TODO Levitation: it is still not implemented
    // Current implementation is a stub
    TaskContext &TC = registerTask();
    Fn(TC);
    return TC.ID;
  }
  bool waitForTasks() {
    // TODO Levitation: it is still not implemented
    // Current implementation is a stub
    for (auto &KV : TasksStatus) {
      if (!KV.second->Successful)
        return false;
    }
    return true;
  }
  bool waitForTasks(const TasksSet &tasksSet) {
    // TODO Levitation: it is still not implemented
    // Current implementation is a stub
    for (auto TID : tasksSet) {
      auto Found = TasksStatus.find(TID);
      if (
        Found != TasksStatus.end() &&
        !Found->second->Successful
      )
        return false;
    }
    return true;
  }

protected:
  TaskContext &registerTask() {
    TaskStatusesAccess.lock();
    with (levitation::make_scope_exit([&] { TaskStatusesAccess.unlock(); })) {
      TaskID TID = NextTaskID++;
      auto Res = TasksStatus.try_emplace(TID, new TaskContext(TID));
      assert(Res.second);
      return *Res.first->second;
    }
  }
};

}}}

#endif //LLVM_CLANG_LEVITATION_TASKSMANAGER_H

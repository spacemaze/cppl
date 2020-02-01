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
#include "clang/Levitation/Common/SimpleLogger.h"
#include "clang/Levitation/Common/WithOperator.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"

#include <deque>
#include <memory>
#include <mutex>
#include <functional>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>

// #define LEVITATION_ENABLE_TASK_MANAGER_LOGS

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

  enum struct TaskStatus {
    Pending = -1,
    Failed = 0,
    Successful = 1
  };

  struct Task {

    Task(TaskID tid, ActionFn &&action)
    : ID(tid), Action(std::move(action))
    {}

    TaskID ID;
    ActionFn Action;
    TaskStatus Status = TaskStatus::Pending;

    bool isPending() {
      return Status == TaskStatus::Pending;
    }
  };

  using TaskPtrTy = std::unique_ptr<Task>;
  using TasksSetInternal = std::unordered_map<TaskID, TaskPtrTy>;

  levitation::log::Logger &Log;

  int JobsNumber;

  std::mutex StatusLocker;
  std::mutex TasksLocker;

  std::condition_variable QueueNotifier;
  std::condition_variable TaskFinishedNotifier;

  int NextTaskID = 0;

  TasksSetInternal Tasks;
  std::deque<TaskID> PendingTasks;

  bool TerminationRequested = false;
  std::unordered_set<std::unique_ptr<std::thread>> Workers;

public:

  TasksManager(int jobsNumber)
  : Log(log::Logger::get()),
    JobsNumber(jobsNumber)
  {
    runWorkers();
  }

  ~TasksManager() {
    {
      auto locker = lockTasks();
      TerminationRequested = true;
    }
    QueueNotifier.notify_all();

    for (auto &w : Workers) {
      log("Joining worker...");
      w->join();
      log("Joining successfull.");
    }
  }

  TaskID addTask(ActionFn &&Fn, bool SameThread = false) {
    // TODO Levitation: it is still not implemented
    // Current implementation is a stub
    return registerTask(std::move(Fn));
  }

  bool waitForTasks(const TasksSet &tasksSet) {

    log("Waiting for some tasks to be completed.");

    auto locker = lockStatus();
    TaskFinishedNotifier.wait(locker, [&] {
      for (auto TID : tasksSet) {
        auto Found = Tasks.find(TID);
        if (Found != Tasks.end() && Found->second->isPending())
          return false;
      }
      return true;
    });

    return true;
  }

  bool waitForTasks() {

    log("Waiting for all tasks to be completed.");

    auto locker = lockStatus();
    TaskFinishedNotifier.wait(locker, [&] {
      log("Checking...");
      for (auto &kv : Tasks) {
        if (kv.second->isPending()) {
          log("Task ", kv.first, " is pending");
          return false;
        }
      }
      log("Checking: All tasks complete!");
      return true;
    });

    log("Waiting task complete.");

    return true;
  }

protected:

  template <typename ...ArgsT>
  void log(ArgsT&&...args) {
#ifdef LEVITATION_ENABLE_TASK_MANAGER_LOGS
    auto _ = Log.lock();
    Log.verbose() << "TaskManager: ";
    log_suffix(std::forward<ArgsT>(args)...);
    Log.verbose() << "\n";
#endif
  }

  void log_suffix() {
  }

  template <typename FirstArgT>
  void log_suffix(FirstArgT Arg) {
    Log.verbose() << Arg;
  }

  template <typename FirstArgT, typename ...ArgsT>
  void log_suffix(FirstArgT first, ArgsT&&...args) {
    log_suffix(first);
    log_suffix(std::forward<ArgsT>(args)...);
  }

  std::unique_lock<std::mutex> lockTasks() {
    return std::unique_lock<std::mutex>(TasksLocker);
  }

  std::unique_lock<std::mutex> lockStatus() {
    return std::unique_lock<std::mutex>(StatusLocker);
  }

  TaskID registerTask(ActionFn &&action) {
    {
      TaskID TID;
      {
        auto tasksLocker = lockTasks();

        TID = NextTaskID++;

        auto Res = Tasks.emplace(TID, new Task(TID, std::move(action)));
        assert(Res.second);

        PendingTasks.push_front(TID);
      }

      log("Registered task ", TID);

      QueueNotifier.notify_one();
      return TID;
    }
  }

  Task *getNextTask(bool *Terminated) {
    auto tasksLocker = lockTasks();

    QueueNotifier.wait(tasksLocker, [&] {
      return TerminationRequested || (bool)PendingTasks.size();
    });

    if (TerminationRequested) {
      *Terminated = true;
      return nullptr;
    }

    auto PendingTaskID = PendingTasks.back();
    PendingTasks.pop_back();

    Task *ptr = Tasks[PendingTaskID].get();
    assert(ptr);

    return ptr;
  }

  template <typename ...ArgsT>
  void logWorker(int Id, ArgsT&&...args) {
    log("Worker[", Id, "]: ", std::forward<ArgsT>(args)...);
  }

  void runWorkers() {
    auto worker = [&] {

      static int Id = 0;
      int MyId = Id++;


      logWorker(MyId, "Launched");

      while (true) {
        bool Terminated = false;
        Task *TaskPtr = getNextTask(&Terminated);

        if (Terminated)
          break;

        Task &Tsk = *TaskPtr;

        logWorker(MyId, "Got task ", Tsk.ID);

        TaskContext context(Tsk.ID);

        Tsk.Action(context);

        {
          auto locker = lockStatus();
          Tsk.Status = context.Successful ?
              TaskStatus::Successful : TaskStatus::Failed;
        }

        logWorker(MyId,
          "Finished task ", Tsk.ID,
          ", status: ",
          (Tsk.Status == TaskStatus::Successful ? "Success" : "Failed")
        );

        TaskFinishedNotifier.notify_all();
      }

      logWorker(MyId, "Stopped");
    };

    for (int j = 0; j != JobsNumber; ++j) {
      Workers.emplace(new std::thread(worker));
    }
  }
};

}}}

#endif //LLVM_CLANG_LEVITATION_TASKSMANAGER_H

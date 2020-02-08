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

#define LEVITATION_ENABLE_TASK_MANAGER_LOGS

namespace clang { namespace levitation { namespace tasks {

class TasksManager : public CreatableSingleton<TasksManager> {
public:
  using TaskID = int;

  struct TaskContext {
    TaskContext(TaskID tid) : ID(tid) {}
    TaskID ID;
    bool Successful = true;
  };

  enum struct TaskStatus {
    Unknown = -3,
    Registered = -2,
    Pending = -1,
    Failed = 0,
    Executing,
    Successful
  };

  using ActionFn = std::function<void(TaskContext&)>;
  using TasksSet = llvm::DenseSet<TaskID>;

private:

  enum struct RegisterAction {
    RegisterOnly,
    Push,
    PushIfHaveFreeWorker
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
  unsigned NumFreeWorkers = 0;
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

  /**
   * Adds new task.
   * @param Fn task action
   * @param SameThread if false, then task will be added into
   * workers queue and execution will be continued.
   * If this flag is false, then task will be registered
   * and executed immediately in same thread.
   * @return task ID
   */
  TaskID addTask(ActionFn &&Fn, bool SameThread = false) {
    auto RegAction = SameThread ?
        RegisterAction::RegisterOnly : RegisterAction::Push;

    Task *Tsk = registerTask(std::move(Fn), RegAction);
    if (SameThread) {
      executeTask(*Tsk);
    }
    return Tsk->ID;
  }

  /**
   * Runs task.
   * If task manager has free workers, then it will add task into
   * queue and continue main thread execution.
   * Otherwise, it will execute task in current thread.
   * @param Fn action to be executed
   * @return task ID
   */
  TaskID runTask(ActionFn &&Fn) {
    Task *Tsk = registerTask(std::move(Fn), RegisterAction::PushIfHaveFreeWorker);
    if (Tsk->Status == TaskStatus::Registered) {
      executeTask(*Tsk);
    }
    return Tsk->ID;
  }

  TaskStatus getTaskStatus(TaskID TID) {
    auto _ = lockStatus();
    auto __ = lockTasks();

    auto Found = Tasks.find(TID);
    assert(Found != Tasks.end());

    return Tasks[TID]->Status;
  }

  bool waitForTasks(const std::initializer_list<TaskID> &v) {
    TasksSet Tasks;
    for (TaskID TID : v) {
      Tasks.insert(TID);
    }
    waitForTasks(Tasks);
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

  log::manipulator_t str(TasksManager::TaskStatus v) {
    return [=] (llvm::raw_ostream &out) {
      switch (v) {
        case TaskStatus::Pending:
          out << "Pending";
          break;
        case TaskStatus::Registered:
          out << "Registered";
          break;
        case TaskStatus::Successful:
          out << "Successful";
          break;
        case TaskStatus::Failed:
          out << "Failed";
          break;
        case TaskStatus::Unknown:
          out << "Unknown";
          break;
        case TaskStatus::Executing:
          out << "Executing";
          break;
      }
    };
  }

  log::manipulator_t str(TasksManager::Task v) {
    return [=] (llvm::raw_ostream &out) {
      out
      << "{ ID:" << v.ID << ", ";
      str(v.Status)(out);
      out
      << "}";
    };
  }

  template <typename ...ArgsT>
  void log(ArgsT&&...args) {
#ifdef LEVITATION_ENABLE_TASK_MANAGER_LOGS
    Log.log_verbose("TaskManager: ", std::forward<ArgsT>(args)...);
#endif
  }

  std::unique_lock<std::mutex> lockTasks() {
    return std::unique_lock<std::mutex>(TasksLocker);
  }

  std::unique_lock<std::mutex> lockStatus() {
    return std::unique_lock<std::mutex>(StatusLocker);
  }

  Task* registerTask(ActionFn &&action, RegisterAction RegAction) {
    {
      Task* TaskPtr = nullptr;
      {
        auto tasksLocker = lockTasks();

        TaskID TID = NextTaskID++;

        TaskPtr = new Task(TID, std::move(action));

        auto Res = Tasks.emplace(TID, TaskPtr);
        assert(Res.second);

        if (
          RegAction == RegisterAction::Push ||
          (
            RegAction == RegisterAction::PushIfHaveFreeWorker &&
            NumFreeWorkers
          )
        ) {
          PendingTasks.push_front(TID);
          TaskPtr->Status = TaskStatus::Pending;
        } else {
          TaskPtr->Status = TaskStatus::Registered;
        }
      }

      log("Registered task ", str(*TaskPtr));

      if (TaskPtr->Status == TaskStatus::Pending)
        QueueNotifier.notify_one();

      return TaskPtr;
    }
  }

  Task *getNextTask(bool *Terminated) {
    auto tasksLocker = lockTasks();

    ++NumFreeWorkers;
    QueueNotifier.wait(tasksLocker, [&] {
      return TerminationRequested || (bool)PendingTasks.size();
    });

    if (TerminationRequested) {
      *Terminated = true;
      return nullptr;
    }
    --NumFreeWorkers;

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

  void executeTask(Task &Tsk) {
    TaskContext context(Tsk.ID);

    {
      auto locker = lockStatus();
      Tsk.Status = TaskStatus::Executing;

      log("Updated task status: ", str(Tsk));
    }

    Tsk.Action(context);

    {
      auto locker = lockStatus();
      Tsk.Status = context.Successful ?
          TaskStatus::Successful : TaskStatus::Failed;

      log("Updated task status: ", str(Tsk));
    }

    TaskFinishedNotifier.notify_all();
  }

  void runWorkers() {
    auto worker = [&] {

      static int Id = 0;
      int MyId = Id++;

      logWorker(MyId, "Launched");

      while (true) {
        bool Terminated = false;
        Task *TaskPtr = getNextTask(&Terminated);

        if (Terminated) {
          break;
        }

        Task &Tsk = *TaskPtr;

        logWorker(MyId, "Got task ", str(Tsk));

        executeTask(Tsk);

        logWorker(MyId, "Finished task ", str(Tsk));
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

//===--- C++ Levitation TasksManager.cpp ------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Tasks manager implementation file
//
//===----------------------------------------------------------------------===//

#include "clang/Levitation/TasksManager/TasksManager.h"
#include <functional>

namespace clang {
namespace levitation {
namespace tasks {

template <>
void inline TasksManager::log_suffix(std::function<void(llvm::raw_ostream &out)> &Arg) {
  Arg(Log.verbose());
}

template <>
std::function<void(llvm::raw_ostream &out)> TasksManager::str(TasksManager::TaskStatus v) {
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

template <>
std::function<void(llvm::raw_ostream &out)> TasksManager::str(const TasksManager::Task &v) {
  return [=] (llvm::raw_ostream &out) {
    out
    << "{ ID:" << v.ID << ", ";
    str(v.Status)(out);
    out
    << "}";
  };
}

}
}
}

//===--- C++ Levitation SimpleLogger.h ------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines C++ Levitation very simple Logger class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_SIMPLELOGGER_H
#define LLVM_CLANG_LEVITATION_SIMPLELOGGER_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

namespace clang { namespace levitation { namespace log {

enum class Level {
  Error,
  Warning,
  Info,
  Verbose
};

class Logger {
  Level LogLevel;
  llvm::raw_ostream &Out;

  Logger(Level LogLevel, llvm::raw_ostream &Out)
  : LogLevel(LogLevel), Out(Out)
  {}

protected:
  static std::unique_ptr<Logger> &accessLoggerPtr() {
    static std::unique_ptr<Logger> LoggerPtr;
    return LoggerPtr;
  }

public:

  static void createLogger(Level LogLevel) {

    llvm::raw_ostream &Out = LogLevel > Level::Warning ?
        llvm::outs() : llvm::errs();

    accessLoggerPtr() = std::unique_ptr<Logger>(new Logger(LogLevel, Out));
  }

  static Logger &get() {
    auto &LoggerPtr = accessLoggerPtr();
    assert(LoggerPtr && "Logger should be created");
    return *LoggerPtr;
  }

  llvm::raw_ostream &error() {
    return getStream(Level::Error);
  }

  llvm::raw_ostream &warning() {
    return getStream(Level::Warning);
  }

  llvm::raw_ostream &info() {
    return getStream(Level::Info);
  }

  llvm::raw_ostream &verbose() {
    return getStream(Level::Verbose);
  }

protected:
  llvm::raw_ostream &getStream(Level ForLevel) {
    if (ForLevel <= LogLevel)
      return Out;
    return llvm::nulls();
  }
};

}}}

#endif //LLVM_CLANG_LEVITATION_SIMPLELOGGER_H

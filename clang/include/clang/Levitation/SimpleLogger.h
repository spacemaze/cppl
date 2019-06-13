// TODO Levitation: Licensing

#ifndef LLVM_CLANG_LEVITATION_SIMPLELOGGER_H
#define LLVM_CLANG_LEVITATION_SIMPLELOGGER_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"
#include <memory>

namespace clang { namespace levitation { namespace log {

enum class Level {
  Error,
  Info,
  Verbose
};

class Log {
  Level LogLevel;
  llvm::raw_ostream &Out;

  Log(Level LogLevel, llvm::raw_ostream &Out)
  : LogLevel(LogLevel), Out(Out)
  {}

public:

  static Log &getLogger(Level LogLevel) {

    static llvm::DenseMap<Level, std::unique_ptr<Log>> Loggers;

    auto InsertionRes = Loggers.insert({LogLevel, nullptr});

    if (InsertionRes.second) {
      llvm::raw_ostream &Out = LogLevel > Level::Error ?
          llvm::outs() : llvm::errs();
      InsertionRes.first->second = std::unique_ptr<Log>(new Log(LogLevel, Out));
    }

    return *InsertionRes.first->second;
  }

  llvm::raw_ostream &error() {
    return getStream(Level::Error);
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

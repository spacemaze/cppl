//===--- Driver.h - C++ Driver class ----------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines C++ Levitation Driver class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LEVITATION_DRIVER_H
#define LLVM_LEVITATION_DRIVER_H

#include "clang/Levitation/DriverDefaults.h"
#include "llvm/ADT/StringRef.h"

namespace clang { namespace levitation { namespace log {
  class Logger;
}}}

namespace clang { namespace levitation { namespace tools {

  class LevitationDriver {

    bool Verbose = false;

    llvm::StringRef SourcesRoot = DriverDefaults::SOURCES_ROOT;
    llvm::StringRef MainSource = DriverDefaults::MAIN_SOURCE;

    llvm::StringRef PreambleSource;

    int JobsNumber = DriverDefaults::JOBS_NUMBER;

    llvm::StringRef OutputHeader;
    llvm::StringRef Output;

    log::Logger &Log;

  public:

    LevitationDriver();

    bool isVerbose() const {
      return Verbose;
    }

    void setVerbose(bool Verbose) {
      LevitationDriver::Verbose = Verbose;
    }

    llvm::StringRef getSourcesRoot() const {
      return SourcesRoot;
    }

    void setSourcesRoot(llvm::StringRef SourcesRoot) {
      LevitationDriver::SourcesRoot = SourcesRoot;
    }

    llvm::StringRef getMainSource() const {
      return MainSource;
    }

    void setMainSource(llvm::StringRef MainSource) {
      LevitationDriver::MainSource = MainSource;
    }

    llvm::StringRef getPreambleSource() const {
      return PreambleSource;
    }

    void setPreambleSource(llvm::StringRef PreambleSource) {
      LevitationDriver::PreambleSource = PreambleSource;
    }

    int getJobsNumber() const {
      return JobsNumber;
    }

    void setJobsNumber(int JobsNumber) {
      LevitationDriver::JobsNumber = JobsNumber;
    }

    llvm::StringRef getOutput() const {
      return Output;
    }

    void setOutput(llvm::StringRef Output) {
      LevitationDriver::Output = Output;
    }

    void setOutputHeader(llvm::StringRef h) {
      OutputHeader = h;
    }

    llvm::StringRef getOutputHeader() const {
      return OutputHeader;
    }

    bool isOutputHeaderCreationRequested() const {
      return OutputHeader.size();
    }

    bool run();

  protected:
    void dumpParameters();
  };
}}}

#endif //LLVM_LEVITATION_DRIVER_H

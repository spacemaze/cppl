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
//  It is a public driver interface. Most of implementation is present
//  in .cpp file as separate classes.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LEVITATION_DRIVER_H
#define LLVM_LEVITATION_DRIVER_H

#include "clang/Levitation/Driver/DriverDefaults.h"
#include "llvm/ADT/StringRef.h"

namespace clang { namespace levitation { namespace log {
  class Logger;
}}}

namespace clang { namespace levitation { namespace tools {

  class LevitationDriver {
  public:
    using Args = llvm::SmallVector<llvm::StringRef, 8>;
  private:

    bool Verbose = false;

    llvm::StringRef SourcesRoot = DriverDefaults::SOURCES_ROOT;
    llvm::StringRef BuildRoot = DriverDefaults::BUILD_ROOT;
    llvm::StringRef MainSource = DriverDefaults::MAIN_SOURCE;

    llvm::StringRef PreambleSource;

    int JobsNumber = DriverDefaults::JOBS_NUMBER;

    llvm::StringRef OutputHeader;
    llvm::StringRef Output;

    bool LinkPhaseEnabled = true;

    bool DryRun;

    Args ExtraParseArgs;
    Args ExtraCodeGenArgs;
    Args ExtraLinkerArgs;

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
      
    void setBuildRoot(llvm::StringRef BuildRoot) {
      LevitationDriver::BuildRoot = BuildRoot;
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

    bool isPreambleCompilationRequested() const {
      return PreambleSource.size();
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

    bool isLinkPhaseEnabled() const {
      return LinkPhaseEnabled;
    }

    void disableLinkPhase() {
      LinkPhaseEnabled = false;
    }

    bool isDryRun() const {
      return DryRun;
    }

    void setDryRun() {
      DryRun = true;
    }

    bool run();

    friend class LevitationDriverImpl;

  protected:

    void initParameters();
    void dumpParameters();
  };
}}}

#endif //LLVM_LEVITATION_DRIVER_H

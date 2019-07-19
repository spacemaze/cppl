//===--- C++ Levitation Driver.cpp ------------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file contains implementation for C++ Levitation Driver methods
//
//===----------------------------------------------------------------------===//

#include "clang/Levitation/Driver.h"
#include "clang/Levitation/SimpleLogger.h"

namespace clang { namespace levitation { namespace tools {

LevitationDriver::LevitationDriver()
: Log(log::Logger::createLogger())
{}

bool LevitationDriver::run() {
  if (Verbose) {
    Log.setLogLevel(log::Level::Verbose);

    dumpParameters();
  }
  return true;
}

void LevitationDriver::dumpParameters() {
  Log.verbose()
  << "\n"
  << "  Running driver with following parameters:\n\n"
  << "    SourcesRoot: " << SourcesRoot << "\n"
  << "    MainSource: " << MainSource << "\n"
  << "    PreambleSource: " << PreambleSource << "\n"
  << "    JobsNumber: " << JobsNumber << "\n"
  << "    Output: " << Output << "\n\n";
}

}}}

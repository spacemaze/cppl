//===--- C++ Levitation DriverDefaults.cpp ------------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file contains default values stubs for C++ Levitation Driver tool
//
//===----------------------------------------------------------------------===//

#include "clang/Levitation/Driver/DriverDefaults.h"

namespace clang { namespace levitation { namespace tools {
  constexpr char DriverDefaults::SOURCES_ROOT[];
  constexpr char DriverDefaults::BUILD_ROOT[];
  constexpr char DriverDefaults::MAIN_SOURCE[];
  constexpr int DriverDefaults::JOBS_NUMBER;
  constexpr char DriverDefaults::OUTPUT_EXECUTABLE[];
  constexpr char DriverDefaults::OUTPUT_OBJECTS_DIR[];
}}}

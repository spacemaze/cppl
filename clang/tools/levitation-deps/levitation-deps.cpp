//===--- C++ Levitation levitation-deps.cpp --------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines C++ Levitation DependenciesSolver tool main file.
//
//===----------------------------------------------------------------------===//

#include "clang/Levitation/ArgsParser.h"
#include "clang/Levitation/DependenciesSolver.h"
#include "clang/Levitation/FileExtensions.h"
#include "clang/Levitation/Serialization.h"

#include <functional>
#include <tuple>

using namespace llvm;
using namespace clang;
using namespace clang::levitation;

static const int RES_WRONG_ARGUMENTS = 1;
static const int RES_FAILED_TO_SOLVE = 2;

static const int RES_SUCCESS = 0;

int main(int argc, char **argv) {

  DependenciesSolver Solver;

  if (
    ArgsParser(
        "C++ Levitation dependencies solver tool",
        argc, argv
    )
    .parameter(
        "-src-root",
        "Specify source root (project) directory.",
        [&](StringRef v) { Solver.setSourcesRoot(v); }
    )
    .parameter(
        "-build-root",
        "Specify build root directory. "
        "Directories structure should repeat project structure.",
        [&](StringRef v) { Solver.setBuildRoot(v); }
    )
    .parameter(
        "-main-file",
        "Specify main source faile, usually 'main.cpp'. ",
        [&](StringRef v) { Solver.setMainFile(v); }
    )
    .optional(
        "--verbose",
        "Enables verbose mode.",
        [&](StringRef v) { Solver.setVerbose(true); }
    )
    .helpParameter("--help", "Shows this help text.")
    .parse<ArgsSeparator::Equal>()
  ) {

    if (!Solver.solve())
      return RES_FAILED_TO_SOLVE;

    return RES_SUCCESS;
  }

  return RES_WRONG_ARGUMENTS;
}
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

#include "clang/Levitation/CommandLineTool/ArgsParser.h"
#include "clang/Levitation/CommandLineTool/CommandLineTool.h"
#include "clang/Levitation/DependenciesSolver.h"
#include "clang/Levitation/FileExtensions.h"
#include "clang/Levitation/Serialization.h"

#include <functional>
#include <tuple>

using namespace llvm;
using namespace clang;
using namespace clang::levitation;
using namespace clang::levitation::command_line_tool;

static const int RES_WRONG_ARGUMENTS = 1;
static const int RES_FAILED_TO_SOLVE = 2;

static const int RES_SUCCESS = 0;

int main(int argc, char **argv) {

  DependenciesSolver Solver;

  return CommandLineTool(argc, argv)
        .description("C++ Levitation dependencies solver tool")
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
            "Specify main source file, usually 'main.cpp'. ",
            [&](StringRef v) { Solver.setMainFile(v); }
        )
        .flag()
            .name("--verbose")
            .description("Enables verbose mode.")
            .action([&](llvm::StringRef) { Solver.setVerbose(true); })
        .done()
        .helpParameter("--help", "Shows this help text.")
        .defaultParser<KeyValueParser>('=')
        .onWrongArgsReturn(RES_WRONG_ARGUMENTS)
        .run([&] {
          if (!Solver.solve())
            return RES_FAILED_TO_SOLVE;

          return RES_SUCCESS;
        });
}
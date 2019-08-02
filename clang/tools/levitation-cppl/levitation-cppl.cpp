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

#include "clang/Levitation/CommandLineTool/CommandLineTool.h"
#include "clang/Levitation/Driver/Driver.h"
#include "clang/Levitation/FileExtensions.h"
#include "clang/Levitation/Common/SimpleLogger.h"
#include "clang/Levitation/Serialization.h"

#include <functional>
#include <tuple>

using namespace llvm;
using namespace clang;
using namespace clang::levitation;
using namespace clang::levitation::command_line_tool;

static const int RES_WRONG_ARGUMENTS = 1;
static const int RES_FAILED_TO_RUN = 2;

static const int RES_SUCCESS = 0;

bool parseJobsNumber(const char **Argv, int &Offset);

int main(int argc, char **argv) {

  clang::levitation::tools::LevitationDriver Driver;

  // Available driver parameters:
  // -root=<path> Specify source root (project) directory. Default value: '.'
  // -main=<path> Specify main source file. Default value: 'main.cpp'
  // -j<N> Maximum jobs number. Default value: 'j1'
  // -h=<path> Header file name to be generated.
  //    If not specified, then header is not generated.
  // -c Compile sources without linking.
  // -o Output file or directory. If -c is not specified,
  //    then it specifies output executable file, with 'a.out' by default.
  //    If -c is specified then it specifies output directory for object files,
  //    with a.dir by default.

  return CommandLineTool<KeyEqValueParser>(argc, argv)
      .description(
          "Is a C++ Levitation Compiler. Depending on mode it's "
          "ran in, it can go through preamble compilation, "
          "initial parsing, dependencies solving, instantiation "
          "and code generation, and finally linker stages."
      )
      .registerParser<KeySpaceValueParser>()
      .registerParser<KeyValueInOneWordParser>()
      .optional(
          "-root", "<directory>",
          "Source root (project) directory.",
          [&](StringRef v) { Driver.setSourcesRoot(v); }
      )
      .optional(
          "-buildRoot", "<directory>",
          "Build root directory.",
          [&](StringRef v) { Driver.setBuildRoot(v); }
      )
      .optional(
          "-main", "<path>",
          "Main source file.",
          [&](StringRef v) { Driver.setMainSource(v); }
      )
      .optional(
          "-preamble", "<path>",
          "Path to preamble. If specified, then preamble compilation stage "
          "will be enabled.",
          [&](StringRef v) { Driver.setPreambleSource(v); }
      )
      .optional(
          "-h", "<path>",
          "Path to header file to be generated. If specified, then header "
          "generation stage will be added to the compilation pipeline.",
          [&](StringRef v) { Driver.setOutputHeader(v); }
      )
      .optional()
          .name("-j")
          .valueHint("<N>")
          .description("Maximum jobs number.")
          .action<int>([&](int v) { Driver.setJobsNumber(v); })
          .useParser<KeyValueInOneWordParser>()
      .done()
      .optional()
          .name("-o")
          .valueHint("<directory>")
          .description(
              "Output file or directory. If -c is not specified,"
              "then it specifies output executable file, with "
              "'a.out' by default. If -c is specified then it "
              "specifies output directory for object files,"
              "with a.dir by default."
          )
          .action([&](StringRef v) { Driver.setOutput(v); })
          .useParser<KeySpaceValueParser>()
      .done()
      .flag()
          .name("-c")
          .description("Compile sources without linking.")
          .action([&](llvm::StringRef) { Driver.disableLinkPhase(); })
      .done()
      .flag()
          .name("--verbose")
          .description("Enables verbose mode.")
          .action([&](llvm::StringRef) { Driver.setVerbose(true); })
      .done()
      .flag()
          .name("-###")
          .description(
              "Toggle dry run mode. "
              "Prints commands to be executed without "
              "execution itself."
          )
          .action([&](llvm::StringRef) { Driver.setDryRun(); })
      .done()
      .helpParameter("--help", "Shows this help text.")
      .onWrongArgsReturn(RES_WRONG_ARGUMENTS)
      .run([&] {
        if (!Driver.run())
          return RES_FAILED_TO_RUN;

        return RES_SUCCESS;
      });
}

bool parseJobsNumber(const char **Argv, int &Offset) {
  return true;
}
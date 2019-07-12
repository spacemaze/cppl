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

#include "clang/Levitation/DependenciesSolver.h"
#include "clang/Levitation/FileExtensions.h"
#include "clang/Levitation/Serialization.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

#include <functional>
#include <tuple>

using namespace llvm;
using namespace clang;
using namespace clang::levitation;

namespace {

  enum class ArgsSeparator {
    Unknown,
    Equal
  };

  using ArgHandleFn = std::function<void(StringRef)>;

  class ArgsParser {
    StringRef AppTitle;
    int Argc;
    char **Argv;

    size_t TitleIndent = 2;
    size_t ParameterNameIndent = 2;
    size_t ParameterDescriptionIndent = 4;
    size_t RightBorder = 70;

    struct Parameter {
      StringRef Name;
      std::string Description;
      ArgHandleFn HandleFunction;
    };

    llvm::SmallVector<StringRef, 16> ParametersInOriginalOrder;
    llvm::DenseMap<StringRef, Parameter> Parameters;

    llvm::DenseSet<StringRef> Visited;
    llvm::DenseSet<StringRef> Optional;

  public:
    ArgsParser(StringRef AppTitle, int Argc, char **Argv)
    : AppTitle(AppTitle), Argc(Argc), Argv(Argv) {}

    ArgsParser& parameter(
        StringRef Name,
        std::string Description,
        ArgHandleFn HandleFunction
    ) {
      Parameters.try_emplace(Name,
          Parameter { Name, std::move(Description), std::move(HandleFunction) }
      );
      ParametersInOriginalOrder.push_back(Name);
      return *this;
    }

    ArgsParser& optional(
        StringRef Name,
        std::string Description,
        ArgHandleFn HandleFunction
    ) {
      Parameters.try_emplace(Name,
          Parameter { Name, std::move(Description), std::move(HandleFunction) }
      );
      ParametersInOriginalOrder.push_back(Name);

      Optional.insert(Name);

      return *this;
    }

    ArgsParser& helpParameter(
        StringRef Name,
        std::string Description
    ) {
      return optional(
          Name, std::move(Description),
          [=] (StringRef v) { printHelp(llvm::outs()); }
      );
    }

    template <ArgsSeparator Separator>
    bool parse() {
      if (Argc == 1) {
        printHelp(llvm::outs());
        return false;
      }

      // Skip first arg, for its command name itself.
      for (int i = 1; i != Argc;) {
        tryParse<Separator>(i);
      }

      bool HasMissedParameters = false;
      for (auto &P : Parameters) {
        auto ParameterName = P.first;

        if (Optional.count(ParameterName))
          continue;

        if (!Visited.count(ParameterName)) {
          reportMissedParameter(ParameterName);
          HasMissedParameters = true;
        }
      }

      if (HasMissedParameters) {
        printHelp(llvm::errs());
        return false;
      }

      return true;
    }

    template<ArgsSeparator S>
    bool tryParse(int &Offset) {
      llvm_unreachable("Not supported");
    }
  private:

    static size_t findMostRightSpace(StringRef S, size_t Start, size_t N) {

      if (S.size() - Start < N)
        return StringRef::npos;

      size_t curSpace = StringRef::npos;
      for (
        size_t nextSpace = S.find(' ', Start);
        nextSpace != StringRef::npos && nextSpace < N;
        nextSpace = S.find(' ', Start)
      ) {
        curSpace = nextSpace;
        Start = nextSpace + 1;
      }
      return curSpace;
    }

    void printDescription(llvm::raw_ostream &out, StringRef Description) const {
      auto Indent = (unsigned)ParameterDescriptionIndent;
      size_t StringWidth = RightBorder - Indent;
      for (
         size_t Start = 0, e = Description.size();
         Start < e;
      ) {
        size_t MostRightSpace = findMostRightSpace(Description, Start, StringWidth);
        bool Wrapped = MostRightSpace != StringRef::npos;

        size_t N = Wrapped ? MostRightSpace - Start : StringWidth;

        StringRef L = Description.substr(Start, N);
        out.indent(Indent) << L << "\n";

        Start += Wrapped ? N + /*space*/1 : N;
      }
    }

    void printParameterHelp(llvm::raw_ostream &out, const Parameter &P) const {
      out.indent(ParameterNameIndent) << P.Name << "\n";
      printDescription(out, P.Description);
      out << "\n";
    }

    void printHelp(llvm::raw_ostream &out) const {
      out << "\n";
      out.indent((unsigned)TitleIndent) << AppTitle << "\n\n";
      for (auto P : ParametersInOriginalOrder) {
        auto Found = Parameters.find(P);
        assert(
            Found != Parameters.end() &&
            "ParametersInOriginalOrder should contain same values as "
            "Parameters keys set."
        );

        printParameterHelp(out, Found->second);
      }
    }
    void reportUnknownParameter(StringRef P) {
      llvm::errs() << "Unknown parameter: '" << P << "'\n";
    }
    void reportMissedParameter(StringRef P) {
      llvm::errs() << "Missed parameter: '" << P << "'\n";
    }
  };

  template<>
  bool ArgsParser::tryParse<ArgsSeparator::Equal>(int &Offset) {
    StringRef Arg = Argv[Offset];

    StringRef Name;
    StringRef Value;

    size_t Eq = Arg.find('=');

    if (Eq != StringRef::npos)
      std::tie(Name, Value) = Arg.split('=');
    else
      Name = Arg;

    auto Found = Parameters.find(Name);

    if (Found != Parameters.end()) {
      Visited.insert(Name);
      Found->second.HandleFunction(Value);
      ++Offset;
      return true;
    }

    reportUnknownParameter(Arg);
    printHelp(llvm::errs());
    return false;
  }
}

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
//===--- Dependencies.h - C++ ArgsParser class ---------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines ArgsParser class which implements simple command
//  line parser.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LEVITATION_ARGSSEPARATOR_H
#define LLVM_LEVITATION_ARGSSEPARATOR_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

namespace clang { namespace levitation { namespace args {
  enum class ValueSeparator {
    Unknown,
    Equal
  };

  using ArgHandleFn = std::function<void(llvm::StringRef)>;

  class ArgsParser {
    llvm::StringRef AppTitle;
    int Argc;
    char **Argv;

    size_t TitleIndent = 2;
    size_t ParameterNameIndent = 2;
    size_t ParameterDescriptionIndent = 4;
    size_t RightBorder = 70;

    struct Parameter {
      llvm::StringRef Name;
      std::string Description;
      ArgHandleFn HandleFunction;
    };

    llvm::SmallVector<llvm::StringRef, 16> ParametersInOriginalOrder;
    llvm::DenseMap<llvm::StringRef, Parameter> Parameters;

    llvm::DenseSet<llvm::StringRef> Visited;
    llvm::DenseSet<llvm::StringRef> Optional;

  public:
    ArgsParser(llvm::StringRef AppTitle, int Argc, char **Argv)
    : AppTitle(AppTitle), Argc(Argc), Argv(Argv) {}

    ArgsParser& parameter(
        llvm::StringRef Name,
        std::string Description,
        ArgHandleFn HandleFunction,
        llvm::StringRef DefaultValue = ""
    ) {
      Parameters.try_emplace(Name,
          Parameter { Name, std::move(Description), std::move(HandleFunction) }
      );
      ParametersInOriginalOrder.push_back(Name);
      return *this;
    }

    ArgsParser& optional(
        llvm::StringRef Name,
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
        llvm::StringRef Name,
        std::string Description
    ) {
      return optional(
          Name, std::move(Description),
          [=] (llvm::StringRef v) { printHelp(llvm::outs()); }
      );
    }

    template <ValueSeparator S>
    bool parse() {
      if (Argc == 1) {
        printHelp(llvm::outs());
        return false;
      }

      // Skip first arg, for its command name itself.
      for (int i = 1; i != Argc;) {
        tryParse<S>(i);
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

    template<ValueSeparator S>
    bool tryParse(int &Offset) {
      llvm_unreachable("Not supported");
    }
  private:

    static size_t findMostRightSpace(llvm::StringRef S, size_t Start, size_t N) {

      if (S.size() - Start < N)
        return llvm::StringRef::npos;

      size_t curSpace = llvm::StringRef::npos;
      for (
        size_t nextSpace = S.find(' ', Start);
        nextSpace != llvm::StringRef::npos && nextSpace < N;
        nextSpace = S.find(' ', Start)
      ) {
        curSpace = nextSpace;
        Start = nextSpace + 1;
      }
      return curSpace;
    }

    void printDescription(llvm::raw_ostream &out, llvm::StringRef Description) const {
      auto Indent = (unsigned)ParameterDescriptionIndent;
      size_t StringWidth = RightBorder - Indent;
      for (
         size_t Start = 0, e = Description.size();
         Start < e;
      ) {
        size_t MostRightSpace = findMostRightSpace(Description, Start, StringWidth);
        bool Wrapped = MostRightSpace != llvm::StringRef::npos;

        size_t N = Wrapped ? MostRightSpace - Start : StringWidth;

        llvm::StringRef L = Description.substr(Start, N);
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
    void reportUnknownParameter(llvm::StringRef P) {
      llvm::errs() << "Unknown parameter: '" << P << "'\n";
    }
    void reportMissedParameter(llvm::StringRef P) {
      llvm::errs() << "Missed parameter: '" << P << "'\n";
    }
  };

  template<>
  bool ArgsParser::tryParse<ValueSeparator::Equal>(int &Offset) {
    llvm::StringRef Arg = Argv[Offset];

    llvm::StringRef Name;
    llvm::StringRef Value;

    size_t Eq = Arg.find('=');

    if (Eq != llvm::StringRef::npos)
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
}}} // end of clang::levitation namespace

#endif //LLVM_LEVITATION_ARGSSEPARATOR_H

//===--- ArgsParser.h - C++ ArgsParser class --------------------*- C++ -*-===//
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

#include "clang/Levitation/CommandLineTool/Parameter.h"
#include "clang/Levitation/CommandLineTool/ParameterValueHandling.h"
#include "clang/Levitation/Failable.h"
#include "clang/Levitation/SimpleLogger.h"
#include "clang/Levitation/WithOperator.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

#include <strstream>

namespace clang { namespace levitation { namespace command_line_tool {

  enum class ValueSeparator {
    Unknown,
    Equal,
    Space
  };

  class ArgumentsParser : public Failable {
  protected:
    llvm::DenseMap<llvm::StringRef, Parameter*> Parameters;

  public:

    virtual ~ArgumentsParser() = default;

    void registerParameter(Parameter *P) {
      auto Res = Parameters.insert({P->Name, P});
      assert(Res.second && "Parameter should be identified by its name.");
    }

    void parse(
        int Argc,
        char **Argv,
        llvm::DenseSet<llvm::StringRef> &VisitedParameters
    ) {
      for (int i = 1; i != Argc;) {
        if (!tryParse(Argc, Argv, i, VisitedParameters))
          ++i;
      }
    }

    virtual bool tryParse(
        int Argc,
        char **Argv,
        int &Offset,
        llvm::DenseSet<llvm::StringRef> &VisitedParameters
    ) = 0;
  };

  class KeyValueParser : public ArgumentsParser {
    char Separator;
  public:

    KeyValueParser(char separator = '=') : Separator(separator) {}

    static llvm::StringRef getName() {
      return "KeyValueParser";
    }

    bool tryParse(
        int Argc,
        char **Argv,
        int &Offset,
        llvm::DenseSet<llvm::StringRef> &VisitedParameters
    ) override {
      llvm::StringRef Arg = Argv[Offset];

      llvm::StringRef Name;
      llvm::StringRef Value;

      size_t Eq = Arg.find(Separator);

      if (Eq != llvm::StringRef::npos)
        std::tie(Name, Value) = Arg.split(Separator);
      else
        Name = Arg;

      auto Found = Parameters.find(Name);

      // TODO Levitation: for flags emit warning, in case if we have
      // non-empty Value

      if (Found != Parameters.end()) {
        Parameter &P = *Found->second;

        VisitedParameters.insert(Name);

        Failable F;
        P.Handler(F, Value);
        if (!F.isValid()) {
          with (auto f = setFailure()) {
            f << "Failed to parse '" << Name << "', " << F.getErrorMessage();
          }
        }

        ++Offset;
        return true;
      }
      return false;
    }
  };
}}} // end of clang::levitation namespace

#endif //LLVM_LEVITATION_ARGSSEPARATOR_H

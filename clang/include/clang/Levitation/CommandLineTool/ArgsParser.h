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

  /// Implements Argument Parsing interface.
  /// Each parses should be known that it may be just an item in parses stack.
  /// Main method is 'parse'. This method accepts Context structure with
  /// details of what is parsed already. This method should also leave corresponding
  /// notes in that Context instance.
  class ArgumentsParser : public Failable {
  protected:
    llvm::DenseMap<llvm::StringRef, Parameter*> Parameters;

  public:

    struct Context {
        int Argc;
        char **Argv;
        llvm::DenseSet<int> VisitedArguments;
        llvm::DenseSet<llvm::StringRef> VisitedParameters;
    };

    virtual ~ArgumentsParser() = default;

    void registerParameter(Parameter *P) {
      auto Res = Parameters.insert({P->Name, P});
      assert(Res.second && "Parameter should be identified by its name.");
    }

    void parse(Context &Ctx) {
      for (int i = 1; i != Ctx.Argc;) {
        // Keep in mind [expr.log.or]
        if (Ctx.VisitedArguments.count(i) || !tryParse(Ctx, i))
          ++i;
      }
    }

    virtual bool tryParse(Context &Ctx, int &Offset) = 0;
  };

  class KeyValueParser : public ArgumentsParser {
    char Separator;
  public:

    KeyValueParser(char separator = '=') : Separator(separator) {}

    bool tryParse(Context &Ctx, int &Offset) override {
      llvm::StringRef Name;
      llvm::StringRef Value;

      int NewOffset = Offset;
      std::tie(Name, Value) = getNameValue(Ctx.Argc, Ctx.Argv, NewOffset, Separator);

      if (!Ctx.VisitedParameters.insert(Name).second)
        return false;

      auto Found = Parameters.find(Name);

      if (Found != Parameters.end()) {
        Parameter &P = *Found->second;
        if (P.IsFlag) {
          // TODO Levitation: for flags emit warning, in case if we have
          // non-empty Value
          Value = llvm::StringRef();
          NewOffset = Offset + 1;
        }

        Failable F;
        P.Handler(F, Value);
        if (!F.isValid()) {
          with (auto f = setFailure()) {
            f << "Failed to parse '" << Name << "', " << F.getErrorMessage();
          }
        }

        // Mark all arguments current parameter consists of as "used"
        for (int va = Offset; va != NewOffset; ++va)
          Ctx.VisitedArguments.insert(va);

        Offset = NewOffset;
        return true;
      }
      return false;
    }

  protected:

    static std::pair<llvm::StringRef, llvm::StringRef> getNameValue(
        int Argc,
        char **Argv,
        int &Offset,
        char S
    ) {
      llvm::StringRef Arg = Argv[Offset++];

      size_t Eq = S != ' ' ? Arg.find(S) : llvm::StringRef::npos;

      if (Eq != llvm::StringRef::npos) {
        return Arg.split(S);
      } else {
        if (Offset < Argc) {
          return std::make_pair(Arg, (llvm::StringRef)Argv[Offset++]);
        }
        return std::make_pair(Arg, llvm::StringRef());
      }
    }
  };

  class KeyEqValueParser : public KeyValueParser {
  public:
    static const char *getName() {
      return "KeyEqValueParser";
    }
    KeyEqValueParser() : KeyValueParser('=') {}
  };

  class KeySpaceValueParser : public KeyValueParser {
  public:
    static const char *getName() {
      return "KeySpaceValueParser";
    }
    KeySpaceValueParser() : KeyValueParser(' ') {}
  };

}}} // end of clang::levitation namespace

#endif //LLVM_LEVITATION_ARGSSEPARATOR_H

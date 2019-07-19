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
    Space,
    InOneWord
  };

  /// Implements Argument Parsing interface.
  /// Each parses should be known that it may be just an item in parses stack.
  /// Main method is 'parse'. This method accepts Context structure with
  /// details of what is parsed already. This method should also leave corresponding
  /// notes in that Context instance.
  class ArgumentsParser {
  protected:
    llvm::DenseMap<llvm::StringRef, Parameter*> Parameters;

  public:

    struct Context {
        int Argc;
        char **Argv;
        Failable &FailableInstance;
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

  /// A tricky way to customize contents of KeyValueParser.
  /// Define in separate class methods we want to specialize.
  /// Why?
  /// 1. Because there is no direct way to specialize one function of
  /// generic template. Only through inharitance, or aggregation (we use latter).
  /// 2. We could use static function templates, but then each .h file user will
  /// use unique function. Whilst usually compiler deal much better with
  /// templates merging.
  /// 3. This way allows to REMOVE this function for unknown template
  /// argument value, and thus emit compile-time errors for unsupported
  /// value separators.
  template<ValueSeparator S>
  struct KeyValueParserStatic {
    // Nothing!
  };

  template<>
  struct KeyValueParserStatic<ValueSeparator::Equal>{
    static const char *getName() {
      return "KeyValueParserEq";
    }

    static std::pair<llvm::StringRef, llvm::StringRef> getNameValue(
        int Argc,
        char **Argv,
        int &Offset
    ) {
      char S = '=';
      llvm::StringRef Arg = Argv[Offset++];

      size_t Eq = Arg.find(S);

      if (Eq != llvm::StringRef::npos) {
        return Arg.split(S);
      }

      return std::make_pair(Arg, llvm::StringRef());
    }
  };

  template<>
  struct KeyValueParserStatic<ValueSeparator::Space>{
    static const char *getName() {
      return "KeyValueParserSpace";
    }

    static std::pair<llvm::StringRef, llvm::StringRef> getNameValue(
        int Argc,
        char **Argv,
        int &Offset
    ) {
      llvm::StringRef Arg = Argv[Offset++];
      if (Offset < Argc) {
        return std::make_pair(Arg, (llvm::StringRef)Argv[Offset++]);
      }
      return std::make_pair(Arg, llvm::StringRef());
    }
  };

  template<>
  struct KeyValueParserStatic<ValueSeparator::InOneWord>{
    static const char *getName() {
      return "KeyValueParserInOneWord";
    }

    static std::pair<llvm::StringRef, llvm::StringRef> getNameValue(
        int Argc,
        char **Argv,
        int &Offset
    ) {
      llvm::StringRef Arg = Argv[Offset++];

      // Format: KeyValue = <name><value>
      //   name: '-'[a-zA-Z]
      //   value: .*
      //
      // E.g. For '-j16'
      //   name is '-j'
      //   value is '16'

      if (Arg.size()) {
        llvm::StringRef Name = Arg.substr(0, 2);
        if (Arg.size() > 1) {
          return std::make_pair(Name, Arg.substr(2));
        }
        return std::make_pair(Name, llvm::StringRef());
      }

      llvm_unreachable("Empty argument is not allowed.");
    }
  };

  template <ValueSeparator Separator>
  class KeyValueParser : public ArgumentsParser {
  public:

    static const char *getName() {
      return KeyValueParserStatic<Separator>::getName();
    }

    bool tryParse(Context &Ctx, int &Offset) override {
      llvm::StringRef Name;
      llvm::StringRef Value;

      int NewOffset = Offset;

      std::tie(Name, Value) = KeyValueParserStatic<Separator>::getNameValue(
          Ctx.Argc, Ctx.Argv, NewOffset
      );

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
          with (auto f = Ctx.FailableInstance.setFailure()) {
            f << "Failed to parse "
            << "'" << Name << "', " << F.getErrorMessage() << ".";
          }
        }

        // Mark all arguments current parameter consists of as "used"
        for (int va = Offset; va != NewOffset; ++va)
          Ctx.VisitedArguments.insert(va);

        Offset = NewOffset;

        if (!Ctx.VisitedParameters.insert(Name).second)
          return false;

        return true;
      }
      return false;
    }
  };

  using KeyEqValueParser = KeyValueParser<ValueSeparator::Equal>;
  using KeySpaceValueParser = KeyValueParser<ValueSeparator::Space>;
  using KeyValueInOneWordParser = KeyValueParser<ValueSeparator::InOneWord>;

}}} // end of clang::levitation namespace

#endif //LLVM_LEVITATION_ARGSSEPARATOR_H

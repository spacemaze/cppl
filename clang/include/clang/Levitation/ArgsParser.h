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

namespace clang { namespace levitation { namespace args {

  class ArgsParser;

  enum class ValueSeparator {
    Unknown,
    Equal,
    Space
  };

  using ValueParserFn = std::function<bool(const char **, int &)>;

  template <typename T>
  using HandleFunctionTy = std::function<void(T)>;

  class ValueHandler : public Failable {
    using OnHandleStrValueFn = std::function<void(Failable &, llvm::StringRef)>;

    OnHandleStrValueFn OnHandleStrValue;
    ValueHandler(OnHandleStrValueFn &&OnHandleStr)
    : OnHandleStrValue(std::move(OnHandleStr))
    {}

  public:
    void Handle(llvm::StringRef v) {
      OnHandleStrValue(*this, v);
    }

    template <typename T>
    static ValueHandler *get(HandleFunctionTy<T> &&HandleFunction) {
      llvm_unreachable("Can't handle this type of value");
    }
  };

  template<>
  ValueHandler *ValueHandler::get<llvm::StringRef>(
      HandleFunctionTy<llvm::StringRef> &&HandleFunction
  ) {
    static ValueHandler DirectStrHandler(
        [&] (Failable &failable, llvm::StringRef v) {
          HandleFunction(v);
        }
    );
    return &DirectStrHandler;
  }

  template<>
  ValueHandler *ValueHandler::get<int>(
      HandleFunctionTy<int> &&HandleFunction
  ) {
    static ValueHandler IntHandler(
        [&] (Failable &failable, llvm::StringRef v) {

          std::istrstream sstrm(v.data(), v.size());

          int IntValue = 0;

          sstrm >> IntValue;

          if (!sstrm.fail())
            HandleFunction(IntValue);
          else {
            with (auto f = failable.setFailure()) {
              f << "Value '" << v << "'is not an integer";
            }
          }
        }
    );
    return &IntHandler;
  };

  struct Parameter {
    llvm::StringRef Name;
    llvm::StringRef HelpTitle;
    llvm::StringRef ValueHint;
    llvm::StringRef Description;

    bool Optional = false;
    bool IsFlag = false;

    ValueHandler *ValueHandler = nullptr;
  };

  class ParameterBuilder {

    std::unique_ptr<Parameter> P;
    ArgsParser &ArgsParserRef;

  public:

    ParameterBuilder(ArgsParser &Parser) : ArgsParserRef(Parser) {
      P = llvm::make_unique<Parameter>();
    }

    ParameterBuilder(ParameterBuilder &&Src)
    : P(std::move(Src.P)),
      ArgsParserRef(Src.ArgsParserRef)
    {}

    ~ParameterBuilder() {

    }

    ParameterBuilder &optional() {
      P->Optional = true;
      return *this;
    }

    ParameterBuilder &flag() {
      P->IsFlag = true;
      return *this;
    }

    ParameterBuilder &name(llvm::StringRef Name) {
      P->Name = Name;
      return *this;
    }

    ParameterBuilder &description(llvm::StringRef Description) {
      P->Description = Description;
      return *this;
    }

    ParameterBuilder &valueHint(llvm::StringRef ValueHint) {
      P->ValueHint = ValueHint;
      return *this;
    }

    ParameterBuilder &helpTitle(llvm::StringRef HelpTitle) {
      P->HelpTitle = HelpTitle;
      return *this;
    }

    template<typename T>
    ParameterBuilder &action(HandleFunctionTy<T> &&Action) {
      P->ValueHandler = ValueHandler::get<T>(std::move(Action));
      return *this;
    }

    ParameterBuilder &action(HandleFunctionTy<llvm::StringRef> &&Action) {
      P->ValueHandler = ValueHandler::get<llvm::StringRef>(std::move(Action));
      return *this;
    }

    ArgsParser &done();
  };

  class ArgsParser : public Failable {

    friend class ParameterBuilder;

    llvm::StringRef AppTitle;
    llvm::StringRef Description;
    int Argc;
    char **Argv;

    size_t TitleIndent = 2;
    size_t ParameterNameIndent = 2;
    size_t ParameterDescriptionIndent = 4;
    size_t RightBorder = 70;

    llvm::SmallVector<llvm::StringRef, 16> ParametersInOriginalOrder;
    llvm::DenseMap<llvm::StringRef, std::unique_ptr<Parameter>> Parameters;

    llvm::DenseSet<llvm::StringRef> Visited;
    llvm::DenseSet<llvm::StringRef> Optional;

    bool HasErrors = false;

  public:
    ArgsParser(llvm::StringRef AppTitle, int Argc, char **Argv)
    : AppTitle(AppTitle), Argc(Argc), Argv(Argv) {}

    ArgsParser& description(llvm::StringRef description) {
      Description = description;
      return *this;
    }

    ArgsParser& parameter(
        llvm::StringRef Name,
        std::string Description,
        HandleFunctionTy<llvm::StringRef> &&HandleFunction,
        llvm::StringRef DefaultValue = ""
    ) {
      return ParameterBuilder(*this)
          .name(Name)
          .description(Description)
          .action(std::move(HandleFunction))
      .done();
    }

    ArgsParser &optional(
        llvm::StringRef Name,
        llvm::StringRef ValueHint,
        llvm::StringRef Description,
        HandleFunctionTy<llvm::StringRef> &&HandleFunction
    ) {
      return ParameterBuilder(*this)
          .optional()
          .name(Name)
          .valueHint(ValueHint)
          .description(Description)
          .action(std::move(HandleFunction))
      .done();
    }

    ParameterBuilder optional() {
      ParameterBuilder Builder(*this);
      Builder.optional();
      return Builder;
    }

    ParameterBuilder flag() {
      ParameterBuilder Builder(*this);
      Builder.flag();
      return Builder;
    }

    ArgsParser& helpParameter(
        llvm::StringRef Name,
        std::string Description
    ) {
      return flag()
          .name(Name)
          .description(Description)
          .action([=] (llvm::StringRef v) { printHelp(llvm::outs()); })
      .done();
    }

    template <ValueSeparator S>
    bool parse() {

      // If we have no parameters passed, then do default action.
      if (Argc == 1) {
        printHelp(llvm::outs());
        return false;
      }

      // Go through command line arguments and parse them.
      // First. We try to use default parsers. And if it fails to parse argument, then...
      // Second. Try parse parameters with custom parsers.
      for (int i = 1; i != Argc;) {
        tryParse<S>(i);
      }

      // Check whether we haven't met some of required parameters.
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

      if (HasErrors || HasMissedParameters) {
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

        printParameterHelp(out, *Found->second);
      }
    }
    void reportUnknownParameter(llvm::StringRef P) {
      llvm::errs() << "Unknown parameter: '" << P << "'\n";
    }
    void reportMissedParameter(llvm::StringRef P) {
      llvm::errs() << "Missed parameter: '" << P << "'\n";
    }
  };

  ArgsParser &ParameterBuilder::done()  {

    if (P->Optional || P->IsFlag)
      ArgsParserRef.Optional.insert(P->Name);

    ArgsParserRef.ParametersInOriginalOrder.push_back(P->Name);
    ArgsParserRef.Parameters.insert({
        P->Name, std::move(P)
    });

    return ArgsParserRef;
  }

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
      Parameter &P = *Found->second;

      if (!P.IsFlag) {
        Visited.insert(Name);
        ValueHandler &ValueHnd = *P.ValueHandler;
        ValueHnd.Handle(Value);

        if (!ValueHnd.isValid()) {
          with (auto f = setFailure()) {
            f << "Failed to parse '" << Name << "', " << ValueHnd.getErrorMessage();
          }
        }
      }

      ++Offset;
      return true;
    }

    reportUnknownParameter(Arg);
    printHelp(llvm::errs());
    return false;
  }

}}} // end of clang::levitation namespace

#endif //LLVM_LEVITATION_ARGSSEPARATOR_H

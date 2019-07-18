//===--- ParameterBuilder.h - C++ ParameterBuilder class --------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines ParameterBuilder class which implements command line tool
//  parameter creation.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LEVITATION_COMMANDLINETOOL_PARAMETERBUILDER_H
#define LLVM_LEVITATION_COMMANDLINETOOL_PARAMETERBUILDER_H

#include "clang/Levitation/CommandLineTool/Parameter.h"
#include "clang/Levitation/CommandLineTool/ParameterValueHandling.h"

#include <functional>
#include <memory>

namespace clang { namespace levitation { namespace command_line_tool {

  template<typename OwnerTy>
  class ParameterBuilder {
  public:
    using OnDoneFn = std::function<OwnerTy&(std::unique_ptr<Parameter> &&P)>;
  private:

    std::unique_ptr<Parameter> P;
    OnDoneFn OnDone;
  public:

    ParameterBuilder(OnDoneFn &&onDone) : OnDone(onDone) {
      P = llvm::make_unique<Parameter>();
    }

    ParameterBuilder(ParameterBuilder &&Src)
    : P(std::move(Src.P)),
      OnDone(Src.OnDone)
    {}

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
    ParameterBuilder &action(ParameterValueHandling::HandleFn<T> &&Action) {
      P->Handler = ParameterValueHandling::get<T>(std::move(Action));
      return *this;
    }

    ParameterBuilder &action(ParameterValueHandling::HandleFn<llvm::StringRef> &&Action) {
      P->Handler = ParameterValueHandling::get<llvm::StringRef>(std::move(Action));
      return *this;
    }

    OwnerTy &done() {
      return OnDone(std::move(P));
    }
  };
}}} // end of clang::levitation::command_line_tool namespace

#endif //LLVM_LEVITATION_COMMANDLINETOOL_PARAMETERBUILDER_H

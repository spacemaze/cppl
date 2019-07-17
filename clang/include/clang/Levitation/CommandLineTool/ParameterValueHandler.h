//===--- ParameterValueHandler.h - C++ ParameterValueHandler class ----------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines ParameterValueHandler class which implements command line tool
//  parameterValueHandler.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LEVITATION_COMMANDLINETOOL_PARAMETERVALUEHANDLER_H
#define LLVM_LEVITATION_COMMANDLINETOOL_PARAMETERVALUEHANDLER_H

#include "clang/Levitation/Failable.h"

#include <strstream>

namespace clang { namespace levitation { namespace command_line_tool {

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

}}} // end of clang::levitation::command_line_tool namespace

#endif //LLVM_LEVITATION_COMMANDLINETOOL_PARAMETERVALUEHANDLER_H

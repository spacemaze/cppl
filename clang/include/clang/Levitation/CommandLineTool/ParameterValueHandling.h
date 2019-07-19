//===--- ParameterValueHandling.h - C++ ParameterValueHandling class ----------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines ParameterValueHandling static class which implements
//  methods and types for values handling.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LEVITATION_COMMANDLINETOOL_PARAMETERVALUEHANDLER_H
#define LLVM_LEVITATION_COMMANDLINETOOL_PARAMETERVALUEHANDLER_H

#include "clang/Levitation/Common/Failable.h"
#include "clang/Levitation/Common/WithOperator.h"

#include <strstream>

namespace clang { namespace levitation { namespace command_line_tool {

  /*static*/
  class ParameterValueHandling : public Failable {
  public:

    template <typename T>
    using HandleFn = std::function<void(T)>;
    using HandleStrFn = std::function<void(Failable &, llvm::StringRef)>;

    template <typename T>
    static HandleStrFn get(HandleFn<T> &&HandleFunction) {
      llvm_unreachable("Can't handle this type of value");
    }
  };

  template<>
  ParameterValueHandling::HandleStrFn
  ParameterValueHandling::get<llvm::StringRef>(
      HandleFn<llvm::StringRef> &&HandleFunction
  ) {
    return  [HandleFunction] (Failable &failable, llvm::StringRef v) {
      HandleFunction(v);
    };
  }

  template<>
  ParameterValueHandling::HandleStrFn ParameterValueHandling::get<int>(
      HandleFn<int> &&HandleFunction
  ) {
    return [HandleFunction] (Failable &failable, llvm::StringRef v) {
      std::istrstream sstrm(v.data(), v.size());

      int IntValue = 0;

      sstrm >> IntValue;

      if (!sstrm.fail())
        HandleFunction(IntValue);
      else {
        with (auto f = failable.setFailure()) {
          f << "value '" << v << "' is not an integer";
        }
      }
    };
  };

}}} // end of clang::levitation::command_line_tool namespace

#endif //LLVM_LEVITATION_COMMANDLINETOOL_PARAMETERVALUEHANDLER_H

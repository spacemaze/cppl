//===--- C++ Levitation StringBuilder.h ------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines string builder class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_STRINGBUILDER_H
#define LLVM_CLANG_LEVITATION_STRINGBUILDER_H

#include "clang/Levitation/WithOperator.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <functional>
#include <string>

namespace clang { namespace levitation {

/// This class is a smart wrapper for raw_string_stream
/// In addition to raw_ostream features it allows to customize
/// action on its destruction, which in turn allows to implement custom
/// string treatment.
///
/// It delegates string creation to raw_ostream by means of
/// template << operator.
class StringBuilder : public WithOperand {
public:
  using OnDoneFn = std::function<void(StringBuilder &)>;
private:
  OnDoneFn OnDone;
  std::string Str;
  llvm::raw_string_ostream Stream;

public:

  StringBuilder(StringBuilder &&Src)
  : OnDone(std::move(Src.OnDone)),
    Str(std::move(Src.Str)),
    Stream(Str)
  {}

  explicit StringBuilder(OnDoneFn &&onDone)
  : OnDone(std::move(onDone)),
    Stream(Str)
  {}

  ~StringBuilder() {
    OnDone(*this);
  }

  template <typename T>
  StringBuilder &operator <<(T v) {
    Stream << v;
    return *this;
  }

  std::string &str() {
    return Str;
  }
};

}}

#endif //LLVM_CLANG_LEVITATION_STRINGBUILDER_H

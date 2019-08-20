//===--- ArgsParser.h - C++ StringOrRef class --------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines "string-or-reference" which can be either a
//  StringRef to some external string or regular std::string
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LEVITATION_STRING_OR_REF_H
#define LLVM_LEVITATION_STRING_OR_REF_H

#include "llvm/ADT/StringRef.h"
#include <string>
#include <utility>

namespace clang { namespace levitation {

  class StringOrRef {
    std::string Storage;
    llvm::StringRef Ref;
  public:

    template <typename StrT>
    /*implicit*/ StringOrRef(const StrT& Str) : Ref(Str) {}

    StringOrRef(const llvm::StringRef& ref)
    : Ref(ref) {}

    StringOrRef(std::string&& str)
    : Storage(std::move(str)), Ref(Storage) {}

    StringOrRef(StringOrRef&& Str) {
      operator=(std::move(Str));
    }

    StringOrRef(const StringOrRef& Str) {
      operator=(Str);
    }

    operator llvm::StringRef() const {
      return Ref;
    }

    StringOrRef& operator=(StringOrRef&& Str) {
      if (Str.Storage.empty()) {
        Ref = Str.Ref;
        return *this;
      }
      Storage = std::move(Str.Storage);
      Ref = llvm::StringRef(Storage);
      return *this;
    }

    StringOrRef& operator=(const StringOrRef& Str) {
      if (Str.Storage.empty()) {
        Ref = Str.Ref;
        return *this;
      }
      Storage = Str.Storage;
      Ref = llvm::StringRef(Storage);
      return *this;
    }
  };

}} // end of clang::levitation namespace

#endif //LLVM_LEVITATION_STRING_OR_REF_H

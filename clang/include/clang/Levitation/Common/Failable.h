//===--- C++ Levitation Failable.h ------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines C++ Levitation failable helper class.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_FAILABLE_H
#define LLVM_CLANG_LEVITATION_FAILABLE_H

#include "clang/Levitation/Common/StringBuilder.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

namespace clang { namespace levitation {

class Failable {
  bool Valid = true;
  llvm::SmallString<80> ErrorMessage;
public:

  void setFailure(llvm::StringRef errorMessage) {
    Valid = false;
    ErrorMessage = errorMessage;
  }

  StringBuilder setFailure() {
    return StringBuilder([&] (StringBuilder &Builder) {
      Valid = false;
      ErrorMessage = Builder.str();
    });
  }

  bool isValid() const { return Valid; }
  llvm::StringRef getErrorMessage() const { return ErrorMessage; }

  // TODO Levitation: getErrorMessages
};

}}

#endif //LLVM_CLANG_LEVITATION_FAILABLE_H

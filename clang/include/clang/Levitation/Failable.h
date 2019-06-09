//
// Created by Stepan Dyatkovskiy on 6/9/19.
//

#ifndef LLVM_CLANG_LEVITATION_FAILABLE_H
#define LLVM_CLANG_LEVITATION_FAILABLE_H

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"

namespace clang { namespace levitation {

class Failable {
  bool Valid = true;
  llvm::SmallString<80> ErrorMessage;
public:

  void setFailure(llvm::StringRef errorMessage) {
    Valid = false;
    ErrorMessage = errorMessage;
  }

  bool isValid() const { return Valid; }
  llvm::StringRef getErrorMessage() const { return ErrorMessage; }
};

}}

#endif //LLVM_CLANG_LEVITATION_FAILABLE_H

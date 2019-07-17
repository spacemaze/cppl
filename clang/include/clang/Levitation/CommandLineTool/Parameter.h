//===--- Parameter.h - C++ Parameter class ----------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines Parameter class which implements command line tool
//  parameter.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LEVITATION_COMMANDLINETOOL_PARAMETER_H
#define LLVM_LEVITATION_COMMANDLINETOOL_PARAMETER_H

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

namespace clang { namespace levitation { namespace command_line_tool {

  class ValueHandler;

  struct Parameter {
    llvm::StringRef Name;
    llvm::StringRef HelpTitle;
    llvm::StringRef ValueHint;
    llvm::StringRef Description;

    bool Optional = false;
    bool IsFlag = false;

    ValueHandler *ValueHandler = nullptr;

    llvm::SmallVector<llvm::StringRef, 4> EnabledForParsers;
  };
}}} // end of clang::levitation::command_line_tool namespace

#endif //LLVM_LEVITATION_COMMANDLINETOOL_PARAMETER_H

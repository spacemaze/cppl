//===--- AlignedPrinter.h - C++ AlignedPrinter class --------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines AlignedPrinter aligned stdout printer
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LEVITATION_ALIGNEDPRINTER_H
#define LLVM_LEVITATION_ALIGNEDPRINTER_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

namespace clang { namespace levitation { namespace command_line_tool {

  class AlignedPrinter {
    int Indent = 0;
    int RightBorder = 0;
    llvm::raw_ostream &Out;

  public:

    AlignedPrinter(llvm::raw_ostream &out) : Out(out) {}

    AlignedPrinter &indent(int indent) {
      Indent = indent;
      return *this;
    }

    AlignedPrinter &rightBorder(int right) {
      RightBorder = right;
      return *this;
    }

    void print(llvm::StringRef Description) {
      size_t StringWidth = RightBorder - Indent;
      for (
         size_t Start = 0, e = Description.size();
         Start < e;
      ) {
        size_t MostRightSpace = findMostRightSpace(Description, Start, StringWidth);
        bool Wrapped = MostRightSpace != llvm::StringRef::npos;

        size_t N = Wrapped ? MostRightSpace - Start : StringWidth;

        llvm::StringRef L = Description.substr(Start, N);
        Out.indent(Indent) << L << "\n";

        Start += Wrapped ? N + /*space*/1 : N;
      }
    }
  protected:
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
  };


}}} // end of clang::levitation::command_line_tool namespace

#endif //LLVM_LEVITATION_ALIGNEDPRINTER_H

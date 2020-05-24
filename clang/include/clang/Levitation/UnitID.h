//===--------- C++ Levitation UnitID.h --------------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines tools for Unit ID manipulations
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_UNITID_H
#define LLVM_CLANG_LEVITATION_UNITID_H

#include "clang/Levitation/Common/Path.h"
#include "clang/Levitation/Common/StringBuilder.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"

namespace clang {
namespace levitation {

class UnitIDUtils {
private:
  static StringRef stripUntil(StringRef Str, char Until) {
    auto pos = Str.find_last_of(Until);
    if (pos != StringRef::npos)
      Str = Str.substr(0, pos);
    return Str;
  }
public:

  static StringRef getComponentSeparator() { return "::"; }

  static std::string fromComponents(
      const llvm::SmallVectorImpl<llvm::StringRef>& Components
  ) {
    StringBuilder sb;
    sb << Components.front();

    auto ComponentSep = getComponentSeparator();

    for (size_t i = 1, e = Components.size(); i != e; ++i)
      sb << ComponentSep << Components[i];

    return sb.str();
  }

  static std::string fromRelPath(StringRef Src) {

    SmallVector<StringRef, 8> Components;
    Src.split(Components, llvm::sys::path::get_separator());

    assert(
        !Components.empty() &&
        "Relative path can't be empty. So at least one component must be present."
    );

    Components.back() = stripUntil(Components.back(), '.');

    return fromComponents(Components);
  }
};

}
}

#endif //LLVM_CLANG_LEVITATION_UNITID_H

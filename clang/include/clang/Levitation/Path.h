//===--- Path.h - C++ Levitation File System Path utils ---------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines additional Path utilities for C++ Levitation project.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_PATH_H
#define LLVM_CLANG_LEVITATION_PATH_H

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include <memory>

namespace llvm {
  class raw_ostream;
  class raw_fd_ostream;
}

namespace clang {
namespace levitation {

using namespace llvm;
  class Path {
  public:
    template <typename SmallStringT>
    static SmallStringT makeRelative(StringRef F, StringRef ParentRel) {
      SmallStringT Relative(F);

      SmallStringT Parent = ParentRel;
      llvm::sys::fs::make_absolute(Parent);

      StringRef Separator = llvm::sys::path::get_separator();

      llvm::sys::path::replace_path_prefix(Relative, Parent, "");

      if (Relative.startswith(Separator))
        Relative = Relative.substr(Separator.size());

      return Relative;
    }
  };
}
}

#endif
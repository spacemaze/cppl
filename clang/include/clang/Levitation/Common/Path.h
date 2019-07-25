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

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
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

  // TODO Levitation: rename to Path
  using SinglePath = llvm::SmallString<256>;
  using Paths = llvm::SmallVector<SinglePath, 64>;

  // TODO Levitation: rename to PathUtils
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

    static void replaceExtension(Paths &paths, StringRef NewExtension) {
      for (auto &P : paths) {
        llvm::sys::path::replace_extension(P, NewExtension);
      }
    }

    static void replaceExtension(Paths &New, const Paths &Src, StringRef NewExtension) {
      New = Src;
      replaceExtension(New, NewExtension);
    }

    static void stripParent(SinglePath &New, const SinglePath &Src, StringRef ParentDir);
    static void stripExtension(SinglePath &New, const SinglePath &Src);

    static void addParent(SinglePath &New, const SinglePath &Src, StringRef ParentDir);
    static void addExtension(SinglePath &New, const SinglePath &Src, StringRef Extension);

    static SinglePath getPath(StringRef NewParent, StringRef SrcRel, StringRef Extension);
  };
}
}

#endif
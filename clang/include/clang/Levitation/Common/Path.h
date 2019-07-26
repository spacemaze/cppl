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

      llvm::sys::path::remove_dots(Relative);

      return Relative;
    }

    template <typename SmallStringT>
    static SmallStringT normalize(StringRef F) {
      SmallStringT Res(F);

      llvm::sys::path::remove_dots(Res);

      return Res;
    }

    /// Builds path out of parent directory, relative path and new extension.
    /// \tparam SmallStringT type which reprents path
    /// \param ParentDir Parent directory to be added in the beginning
    /// \param SrcRel Relative path
    /// \param Extension New extension.
    /// \return
    template <typename SmallStringT>
    static SmallStringT getPath(
        StringRef ParentDir, StringRef SrcRel, StringRef Extension
    ) {
      assert(
          llvm::sys::path::is_relative(SrcRel) &&
          "Path should be relative"
      );

      SmallStringT Res = ParentDir;
      llvm::sys::path::append(Res, SrcRel);
      llvm::sys::path::replace_extension(Res, Extension);
      return Res;
    }
  };
}
}

#endif
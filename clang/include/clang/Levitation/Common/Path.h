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

#include "clang/Levitation/Common/StringsPool.h"
#include "clang/Levitation/Common/StringBuilder.h"
#include "llvm/ADT/DenseSet.h"
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

// FIXME Levitation: get rid of that using directive.
//   In .h files it's a bad practice to make namespaces usings.
using namespace llvm;

  // TODO Levitation: rename to Path
  using SinglePath = llvm::SmallString<256>;
  using Paths = llvm::SmallVector<SinglePath, 64>;
  using PathsPoolTy = StringsPool<256>;
  using PathIDsSet = DenseSet<StringID>;

  // TODO Levitation: rename to PathUtils
  class Path {
  public:

    static bool hasParent(StringRef Source, StringRef ParentRel) {
      SinglePath ParentAbs = ParentRel;
      llvm::sys::path::remove_dots(ParentAbs);
      llvm::sys::fs::make_absolute(ParentAbs);

      SinglePath SourceAbs = Source;
      llvm::sys::path::remove_dots(SourceAbs);
      llvm::sys::fs::make_absolute(SourceAbs);

      return ((StringRef)SourceAbs).startswith(ParentAbs);
    }

    template <typename SmallStringT>
    static SmallStringT makeAbsolute(StringRef F) {
      SmallStringT Relative(F);
      llvm::sys::fs::make_absolute(Relative);
      llvm::sys::path::remove_dots(Relative, /*remove ..*/true);
      return Relative;
    }

    template <typename SmallStringT>
    static SmallStringT makeRelative(StringRef F, StringRef ParentRel) {
      SmallStringT Relative(F);
      llvm::sys::fs::make_absolute(Relative);
      llvm::sys::path::remove_dots(Relative, /*remove ..*/ true);

      SmallStringT Parent = ParentRel;
      llvm::sys::fs::make_absolute(Parent);
      llvm::sys::path::remove_dots(Parent,  /*remove ..*/ true);

      StringRef Separator = llvm::sys::path::get_separator();

      llvm::sys::path::replace_path_prefix(Relative, Parent, "");

      if (Relative.startswith(Separator))
        Relative = Relative.substr(Separator.size());

      llvm::sys::path::remove_dots(Relative, /*remove ..*/ true);

      return Relative;
    }

    template <typename SmallStringT>
    static SmallStringT normalize(StringRef F) {
      SmallStringT Res(F);

      llvm::sys::path::remove_dots(Res);

      return Res;
    }

    /// Replaces extension for given file path
    /// \tparam SmallStringT type which reprents path
    /// \param Src File path (may be with or without any extension)
    /// \param Extension New extension.
    /// \return
    template <typename SmallStringT>
    static SmallStringT replaceExtension(
        StringRef Src, StringRef Extension
    ) {
      SmallStringT Res = Src;
      llvm::sys::path::replace_extension(Res, Extension);
      return Res;
    }

    /// Builds path out of parent directory, relative path and new extension.
    /// \tparam SmallStringT type which reprents path
    /// \param ParentDir Parent directory to be added in the beginning
    ///                  (appliable if path is not absolute)
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

    /// Builds path out of parent directory and relative path
    /// \tparam SmallStringT type which reprents path
    /// \param ParentDir Parent directory to be added in the beginning
    /// \param SrcRel Relative path
    /// \return
    template <typename SmallStringT>
    static SmallStringT getPath(
        StringRef ParentDir, StringRef SrcRel
    ) {
      assert(
          llvm::sys::path::is_relative(SrcRel) &&
          "Path should be relative"
      );

      SmallStringT Res = ParentDir;
      llvm::sys::path::append(Res, SrcRel);
      return Res;
    }

    static void createDirsForFile(StringRef FilePath) {
      auto Parent = llvm::sys::path::parent_path(FilePath);
      llvm::sys::fs::create_directories(Parent);
    }

    class Builder {
      SinglePath Result;
      bool Done = false;
      public:
        Builder(StringRef Prefix = "") : Result(Prefix) {}

        Builder& addComponent(StringRef Component) {
          // FIXME Levitation: support absolute paths
          //     support windows absolute paths.
          assert(!Done);
          llvm::sys::path::append(Result, Component);
          return *this;
        }

        const SinglePath& str() {
          assert(Done);
          return Result;
        }

        void done() {
          Done = true;
        }

        void done(SinglePath &Dest) {
          assert(!Done);
          Done = true;
          Dest.swap(Result);
          Result.clear();
        }

        Builder& replaceExtension(StringRef Extension) {
          assert(!Done);
          Result = Path::replaceExtension<SinglePath>(Result, Extension);
          return *this;
        }
    };
  };
}
}

#endif
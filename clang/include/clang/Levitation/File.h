//===--- C++ Levitation File.h ----------------------------------- -----*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines C++ Levitation File class, which implements
//  output file stream wrapper. Its code is based on related LLVM code inlined
//  mostly in CompilerInvocation.cpp methods and its satellites.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_FILE_H
#define LLVM_CLANG_LEVITATION_FILE_H

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

class File {
  public:
    enum StatusEnum {
      Good,
      HasStreamErrors,
      FiledToRename,
      FailedToCreateTempFile
    };

  private:
    StringRef TargetFileName;
    SmallString<128> TempPath;
    std::unique_ptr<llvm::raw_fd_ostream> OutputStream;
    StatusEnum Status;
  public:
    File(StringRef targetFileName)
      : TargetFileName(targetFileName), Status(Good) {}

    // TODO Levitation: we could introduce some generic template,
    //   something like scope_exit, but with ability to convert it to bool.
    class FileScope {
        File *F;
    public:
        FileScope(File *f) : F(f) {}
        FileScope(FileScope &&src) : F(src.F) { src.F = nullptr; }
        ~FileScope() { if (F) F->close(); }

        operator bool() const { return F; }
        llvm::raw_ostream& getOutputStream() { return *F->OutputStream; }
    };

    FileScope open() {
      // Write to a temporary file and later rename it to the actual file, to avoid
      // possible race conditions.

      StringRef Dir = llvm::sys::path::parent_path(TargetFileName);
      llvm::sys::fs::create_directories(Dir);

      TempPath = TargetFileName;
      TempPath += "-%%%%%%%%";
      int fd;

      if (llvm::sys::fs::createUniqueFile(TempPath, fd, TempPath)) {
        Status = FailedToCreateTempFile;
        return FileScope(nullptr);
      }

      OutputStream.reset(new llvm::raw_fd_ostream(fd, /*shouldClose=*/true));

      return FileScope(this);
    }

    void close() {
      OutputStream->close();
      if (OutputStream->has_error()) {
        OutputStream->clear_error();
        Status = HasStreamErrors;
      }

      if (llvm::sys::fs::rename(TempPath, TargetFileName)) {
        llvm::sys::fs::remove(TempPath);
        Status = FiledToRename;
      }
    }

    bool hasErrors() const {
      return Status != Good;
    }

    StatusEnum getStatus() const {
      return Status;
    }
  };
}
}

#endif
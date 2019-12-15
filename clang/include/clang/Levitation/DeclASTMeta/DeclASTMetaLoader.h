//===--- DeclASTMeta.h - C++ DeclASTMeta class ------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file contains root clas for Decl AST meta data
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LEVITATION_DECLASTMETALOADER_H
#define LLVM_LEVITATION_DECLASTMETALOADER_H

#include "clang/Basic/FileManager.h"
#include "clang/Levitation/DeclASTMeta/DeclASTMeta.h"
#include "clang/Levitation/Common/CreatableSingleton.h"
#include "clang/Levitation/Common/Failable.h"
#include "clang/Levitation/Common/File.h"
#include "clang/Levitation/Common/Path.h"
#include "clang/Levitation/Common/SimpleLogger.h"
#include "clang/Levitation/Serialization.h"

#include "llvm/Support/MemoryBuffer.h"

namespace clang { namespace levitation {
  class DeclASTMetaLoader {

  public:

    static bool fromFile(
        DeclASTMeta &Meta, StringRef BuildRoot, StringRef FileName
    ) {
      auto &FM = CreatableSingleton<FileManager>::get();

      if (auto Buffer = FM.getBufferForFile(FileName)) {
        llvm::MemoryBuffer &MemBuf = *Buffer.get();

        if (!fromBuffer(Meta, MemBuf))
          log::Logger::get().error()
          << "Failed to read dependencies for '" << FileName << "'\n";
      } else
       log::Logger::get().error() << "Failed to open file '" << FileName << "'\n";

      return true;
    }

    static bool fromBuffer(DeclASTMeta &Meta, const MemoryBuffer &MemBuf) {
      auto Reader = CreateMetaBitstreamReader(MemBuf);

      if (!Reader->read(Meta)) {
        log::Logger::get().error()
        << Reader->getStatus().getErrorMessage() << "\n";
        return false;
      }

      if (Reader->getStatus().hasWarnings()) {
        log::Logger::get().warning()
        << Reader->getStatus().getWarningMessage() << "\n";
      }

      return true;
    }
  };
}}

#endif //LLVM_LEVITATION_DECLASTMETALOADER_H

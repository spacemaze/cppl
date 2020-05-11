//===--- Driver.h - C++ PackageFiles class --------------------*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file contains PackageFiles class which implements storage files
//  information for package.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LEVITATION_PACKAGEFILES_H
#define LLVM_LEVITATION_PACKAGEFILES_H

#include "llvm/ADT/DenseMap.h"
#include "clang/Levitation/Common/Path.h"
#include "clang/Levitation/Common/SimpleLogger.h"
#include "clang/Levitation/Common/StringsPool.h"
#include <memory>

namespace clang { namespace levitation { namespace tools {

  struct FilesInfo {

    SinglePath Source;
    SinglePath Header;
    SinglePath Decl;
    SinglePath LDeps;
    SinglePath LDepsMeta;
    SinglePath DeclASTMetaFile;
    SinglePath ObjMetaFile;

    SinglePath DeclAST;
    SinglePath Object;

    void dump(log::Logger &Log, log::Level Level, unsigned indent = 0) {

      std::string StrIndent(indent, ' ');

      Log.log(Level, StrIndent, "Source: ", Source);
      Log.log(Level, StrIndent, "Header: ", Header);
      Log.log(Level, StrIndent, "LDeps: ", LDeps);
      Log.log(Level, StrIndent, "LDepsMeta: ", LDepsMeta);
      Log.log(Level, StrIndent, "DeclASTMetaFile: ", DeclASTMetaFile);
      Log.log(Level, StrIndent, "ObjMetaFile: ", ObjMetaFile);
      Log.log(Level, StrIndent, "DeclAST: ", DeclAST);
      Log.log(Level, StrIndent, "Object: ", Object);

    }
  };

  class FilesMapTy {
  public:
    using UniquePtrMap = llvm::DenseMap<StringID, std::unique_ptr<FilesInfo>>;
  private:
    UniquePtrMap FilesMap;
  public:

    FilesInfo& create(StringID PackageID) {
      auto Res = FilesMap.insert({ PackageID, std::make_unique<FilesInfo>() });
      assert(Res.second);

      return *Res.first->second;
    }

    UniquePtrMap::size_type count(StringID PackageID) const {
      return FilesMap.count(PackageID);
    }

    const FilesInfo* tryGet(StringID PackageID) const {
      static std::unique_ptr<FilesInfo> Empty;
      auto Found = FilesMap.find(PackageID);

      if (Found != FilesMap.end())
        return Found->second.get();
      return nullptr;
    }

    const FilesInfo& operator[](StringID PackageID) const {
      auto Ptr = tryGet(PackageID);
      if (Ptr)
        return *Ptr;
      llvm_unreachable("FileInfo not found.");
    }

    const UniquePtrMap& getUniquePtrMap() const {
      return FilesMap;
    }
  };
}}}

#endif //LLVM_LEVITATION_PACKAGEFILES_H

//===--- Dependencies.h - C++ Levitation Depencency classes -----*- C++ -*-===//
//
// Part of the C++ Levitation Project,
// under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file defines C++ Levitation PackageDependency class and
//  its satellites.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LEVITATION_DEPENDENCIES_H
#define LLVM_CLANG_LEVITATION_DEPENDENCIES_H

#include "clang/Basic/SourceLocation.h"
#include "clang/Levitation/Common/Path.h"
#include "clang/Levitation/Common/IndexedSet.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"

namespace llvm {
  class raw_ostream;
}

namespace clang {

  class DiagnosticBuilder;
  class NamedDecl;

namespace levitation {

  /// Implements package dependency metadata
  typedef ArrayRef<StringRef> DependencyComponentsArrRef;
  typedef SmallVector<StringRef, 16> DependencyComponentsVector;

  class PackageDependencyBuilder;

  /// Describes particular dependency used
  /// in given package.
  class PackageDependency {
  protected:

    /// Location of #import directive
    SourceRange ImportLocation;

    /// Dependency components. E.g. for global::A::B
    /// components vector will store {A, B}
    DependencyComponentsVector Components;

    /// Stores path to dependency file.
    /// This value is set during validation process or
    /// in case of manual dependencies declaration (not implemented yet)
    SinglePath Path;

    PackageDependency() {};

  public:
    friend class PackageDependencyBuilder;

    PackageDependency(DependencyComponentsVector &&components)
    : Components(components) {}

    PackageDependency(PackageDependency &&dying)
    : ImportLocation(dying.ImportLocation),
      Components(std::move(dying.Components)),
      Path(std::move(dying.Path)) {}

    void setImportLoc(const SourceRange &Loc) { ImportLocation = Loc; }

    const SourceRange &getImportLoc() const { return ImportLocation; }

    DependencyComponentsArrRef getComponents() const {
      return Components;
    }

    void setPath(SinglePath &&path) {
      Path.swap(path);
    }

    StringRef getPath() const {
      return StringRef(Path.begin(), Path.size());
    }

    void print(llvm::raw_ostream &out) const;
  };

  class PackageDependencyBuilder {
    PackageDependency Dependency;
  public:
    void addComponent(const StringRef& Component) {
      Dependency.Components.push_back(Component);
    }

    void setImportLoc(const SourceRange &Loc) {
      Dependency.setImportLoc(Loc);
    }

    PackageDependency& getDependency() {
      return Dependency;
    }
  };

  /// Dependencies map.
  /// Implements indexing of package dependency by its component.
  /// E.g. for 'class C { global::A::D d; };' it will add
  /// dependency "A::D".
  class DependenciesMap : public llvm::DenseMap<
  // FIXME Levitation: this is very unstable definition,
  // Key is a ref to PackageDependency's components SmallVector,
  // after the first internal DenseMap::moveFromOldBuckets call
  // PackageDependency will be destroyed safe it is moved first
  // into the new allocated value place, so vector data won't
  // be destroyed in this case, unless you're using
  // PackageDependency + SmallVector with implemented move constructor
  // on both sides. This is why it is unstable.
          DependencyComponentsArrRef, PackageDependency> {
  public:
      // Note: we rely on implicitly defined move constructor here.
      // FIXME Levitation: we don't need it for manual import.
      void mergeDependency(PackageDependency &&Dep);
  };

  /// Almost same as DependenciesMap, but also adds information
  /// about dependency file whenever it is possible.
  class ValidatedDependenciesMap : public DependenciesMap {
    bool HasMissingDependencies = false;
  public:
    void setHasMissingDependencies() { HasMissingDependencies = true; }
    bool hasMissingDependencies() { return HasMissingDependencies; }
  };

  /// Describes whole set of dependencies used by
  /// given package.
  struct PackageDependencies {
    ValidatedDependenciesMap DeclarationDependencies;
    ValidatedDependenciesMap DefinitionDependencies;
    StringRef PackageFilePath;
    bool IsPublic;
    bool hasMissingDependencies() {
      return DeclarationDependencies.hasMissingDependencies() ||
             DefinitionDependencies.hasMissingDependencies();
    }
  };

  const DiagnosticBuilder &operator<<(
      const DiagnosticBuilder &DB,
      const PackageDependency &V
  );
}
}

#endif
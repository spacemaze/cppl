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
#include "clang/Levitation/IndexedSet.h"
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
  typedef SmallString<256> DependencyPath;

  /// Stores information about dependency user
  struct PackageDependencyUse {
      const NamedDecl *DependentNamedDecl;
      SourceRange Location;
  };
  typedef SmallVector<PackageDependencyUse, 8> DependencyUsesVector;

  class PackageDependencyBuilder;

  /// Describes particular dependency used
  /// in given package.
  class PackageDependency {
  protected:

    /// Describes places where this dependency is used.
    DependencyUsesVector Uses;

    /// Dependency components. E.g. for global::A::B
    /// components vector will store {A, B}
    DependencyComponentsVector Components;

    /// Stores path to dependency file.
    /// This value is set during validation process or
    /// in case of manual dependencies declaration (not implemented yet)
    DependencyPath Path;

    PackageDependency() {};

  public:
    friend class PackageDependencyBuilder;

    PackageDependency(DependencyComponentsVector &&components)
    : Components(components) {}

    PackageDependency(PackageDependency &&dying)
    : Uses(std::move(dying.Uses)),
      Components(std::move(dying.Components)),
      Path(std::move(dying.Path)) {}

    void addUse(const NamedDecl *ND, const SourceRange& Loc) {
      Uses.push_back({ND, Loc});
    }

    void addUse(const PackageDependencyUse &Use) {
      Uses.push_back(Use);
    }

    void addUses(const DependencyUsesVector& src) {
      Uses.append(src.begin(), src.end());
    }

    const DependencyUsesVector &getUses() const {
      return Uses;
    }

    const PackageDependencyUse& getFirstUse() const {
      assert (Uses.size() && "Uses collection expected to have at least one use");
      return Uses.front();
    }

    const PackageDependencyUse& getSingleUse() const {
      if (Uses.size() == 1)
        return getFirstUse();
      llvm_unreachable("Uses contains more that one use.");
    }

    DependencyComponentsArrRef getComponents() const {
      return Components;
    }

    void setPath(DependencyPath &&path) {
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

    void addUse(const NamedDecl *ND, const SourceRange& Loc) {
      Dependency.addUse(ND, Loc);
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
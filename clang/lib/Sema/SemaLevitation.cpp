//===------- SemaLevitation.cpp - Semantic Analysis for C++ Levitation ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis for C++ Levitation.
//===----------------------------------------------------------------------===//

#include "clang/Sema/Sema.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/ASTMutationListener.h"
#include "clang/AST/NestedNameSpecifier.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Sema/SemaInternal.h"
#include "clang/Sema/Template.h"
#include "clang/Lex/Preprocessor.h"

#include <iterator>
using namespace clang;
using namespace sema;

//===--------------------------------------------------------------------===//
// Helpers
//

template<typename T>
using ReversedVectorItems = llvm::iterator_range<typename SmallVectorImpl<T>::reverse_iterator>;

template<typename T>
ReversedVectorItems<T> reverse(SmallVectorImpl<T>& Vector) {
  typedef SmallVectorImpl<T> VectorTy;
  return llvm::iterator_range<typename VectorTy::reverse_iterator>(Vector.rbegin(), Vector.rend());
}

levitation::PackageDependency makePackageDependency(
    const SmallVectorImpl<llvm::StringRef> &DepIdParts,
    const SourceRange &Loc
) {
  levitation::PackageDependencyBuilder DependencyBuilder;
  for (const auto &Component : DepIdParts) {
    DependencyBuilder.addComponent(Component);
  }

  DependencyBuilder.setImportLoc(Loc);

  return std::move(DependencyBuilder.getDependency());
}

void Sema::HandleLevitationPackageDependency(
    const SmallVectorImpl<llvm::StringRef> &DepIdParts,
    bool IsBodyDependency,
    const SourceRange &Loc) {
  auto Dependency = makePackageDependency(DepIdParts, Loc);
  if (IsBodyDependency)
    LevitationDefinitionDependencies.mergeDependency(std::move(Dependency));
  else
    LevitationDeclarationDependencies.mergeDependency(std::move(Dependency));
}

void Sema::ActOnLevitationManualDeps() {
  for (const auto &DepParts : PP.getLevitationDeclDeps())
    HandleLevitationPackageDependency(DepParts.first, false, DepParts.second);

  for (const auto &DepParts : PP.getLevitationBodyDeps())
    HandleLevitationPackageDependency(DepParts.first, true, DepParts.second);
}

bool Sema::levitationMayBeSkipVarDefinition(
    const Declarator &D,
    const DeclContext *DC,
    bool IsVariableTemplate,
    clang::StorageClass SC) const {

  // FIXME Levitation: check for SkipFunctionBodies flag.

  if (!CurContext->isFileContext())
    return false;

  bool IsStaticMember = DC->isRecord();
  bool IsFileVar = DC->isFileContext();

  // Skip initialization of static non-template data members and global variables
  // defined without "static" keyword.
  // But preserve initialization for global vars defined with static.
  bool SkipInit = IsStaticMember ?
      !IsVariableTemplate && !DC->isDependentContext() :
      IsFileVar && !IsVariableTemplate && SC != StorageClass ::SC_Static;

  return SkipInit;
}

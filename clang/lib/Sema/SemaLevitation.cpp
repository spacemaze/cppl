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
#include "llvm/ADT/DenseMap.h"

#include <iterator>
#include <utility>

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

bool Sema::isLevitationFilePublic() const {
  // See task: #49
  llvm_unreachable("not-implemented");
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
    bool IsRedeclaration,
    clang::StorageClass SC) {

  if (!isLevitationMode(
      LangOptions::LBSK_BuildPreamble,
      LangOptions::LBSK_BuildDeclAST
  ))
    return false;

  if (!CurContext->isFileContext())
    return false;

  bool IsStaticMember = DC->isRecord();
  bool IsFileVar = DC->isFileContext();
  bool IsStatic =
    SC == SC_Static ||
    (
      SC != StorageClass::SC_Extern &&
      D.getDeclSpec().getConstSpecLoc().isValid()
    );

  auto SkipAction = LevitationVarSkipAction::None;

  if (!IsVariableTemplate) {
    if (IsStaticMember && !DC->isDependentContext()) {
      SkipAction = LevitationVarSkipAction::Skip;
    } else if (IsFileVar) {
      if (IsRedeclaration) {
        // For continue parsing for static redeclarations,
        // that should force diagnostics, for it is a wrong static use-case.
        if (!IsStatic)
          SkipAction = LevitationVarSkipAction::Skip;
      } else
        SkipAction = LevitationVarSkipAction::SkipInit;
    }
  }

  if (SkipAction != LevitationVarSkipAction::None) {
    LevitationVarSkipActions.try_emplace(D, SkipAction);
    if (SkipAction == LevitationVarSkipAction::Skip)
      return true;
  }

  return false;
}

Sema::LevitationVarSkipAction Sema::levitationGetSkipActionFor(
    const Declarator &D
) {
  auto Found = LevitationVarSkipActions.find(D);
  if (Found != LevitationVarSkipActions.end())
    return Found->second;
  return LevitationVarSkipAction::None;
}

void Sema::levitationAddSkippedSourceFragment(
    const clang::SourceLocation &Start,
    const clang::SourceLocation &End,
    bool ReplaceWithSemicolon
) {

  auto StartSLoc = getSourceManager().getDecomposedLoc(Start);
  auto EndSLoc = getSourceManager().getDecomposedLoc(End);

  auto MainFileID = getSourceManager().getMainFileID();
  assert(
      StartSLoc.first == MainFileID &&
      EndSLoc.first == MainFileID &&
      "Skipped fragment can only be a part of main file."
  );

  LevitationSkippedFragments.push_back({
    StartSLoc.second,
    EndSLoc.second,
    ReplaceWithSemicolon
  });

#if 1
  llvm::errs() << "Added skipped fragment "
               << (ReplaceWithSemicolon ? "BURN:\n" : ":\n");

  llvm::errs() << "Bytes: 0x";
  llvm::errs().write_hex(StartSLoc.second) << " : 0x";
  llvm::errs().write_hex(EndSLoc.second) << "\n";

  Start.dump(getSourceManager());
  End.dump(getSourceManager());

  llvm::errs() << "\n";
#endif
}
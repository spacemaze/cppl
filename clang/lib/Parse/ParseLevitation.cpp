//===---- ParseLevitation.cpp - Parser customization fir C++ Levitation ---===//
//
// Part of the C++ Levitation Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//
//
//  This file implements additional parser methods for C++ Levitation mode.
//===----------------------------------------------------------------------===//

#include "clang/Parse/Parser.h"

#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Parse/RAIIObjectsForParser.h"
#include "clang/Sema/Sema.h"

#include "llvm/ADT/SmallVector.h"

namespace clang {
  class Scope;
  class BalancedDelimiterTracker;
  class DiagnosticBuilder;
}

using namespace llvm;
using namespace clang;

/// By default whenever we parse C++ Levitation files,
/// we're in unit's namespace.
void Parser::LevitationEnterUnit(SourceLocation Start, SourceLocation End) {
  StringRef UnitIDStr = getPreprocessor().getPreprocessorOpts().LevitationUnitID;

  if (LevitationUnitID.empty())
    UnitIDStr.split(LevitationUnitID, "::");

  // As long as final unit component is a file name,
  // it can't be empty.
  if (LevitationUnitID.empty())
    llvm_unreachable("C++ Levitation Unit ID can't be empty");

  // It is only allowed to enter unit from global scope.
  // So scopes stack should be empty.
  if (!LevitationUnitScopes.empty())
    llvm_unreachable("C++ Levitation Unit can be started only from global scope");

  // Unit start location coincidents with first met declaration
  //
  SourceLocation UnitLoc = Start;
  ParsedAttributesWithRange attrs(AttrFactory);
  UsingDirectiveDecl *ImplicitUsingDirectiveDecl = nullptr;

  for (StringRef Component : LevitationUnitID) {
    auto *CompIdent = getPreprocessor().getIdentifierInfo(Component);

    LevitationUnitScopes.emplace_back(this);

    LevitationUnitScopes.back().Namespace = cast<NamespaceDecl>(
        Actions.ActOnStartNamespaceDef(
          getCurScope(),
          /*InlineLoc=*/SourceLocation(),
          /*Namespace location=*/UnitLoc,
          /*Ident loc=*/UnitLoc,
          /*Ident=*/CompIdent,
          /*LBrace=*/UnitLoc,
          /*AttrList=*/attrs,
          /*UsingDecl=*/ImplicitUsingDirectiveDecl
        )
    );
  }

  Actions.levitationActOnEnterUnit(Start, End);
}

void Parser::LevitationLeaveUnit(SourceLocation Start, SourceLocation End) {
  if (LevitationUnitScopes.empty())
    llvm_unreachable("Unit Scope items info should not be empty.");

  SourceLocation LeaveUnitLoc = Tok.getLocation();

  // Leave scope in reverse order.
  while (!LevitationUnitScopes.empty()) {
    auto &ScopeItem = LevitationUnitScopes.back();

    ScopeItem.Scope->Exit();
    Actions.ActOnFinishNamespaceDef(ScopeItem.Namespace, LeaveUnitLoc);

    LevitationUnitScopes.pop_back();
  }

  Actions.levitationActOnLeaveUnit(Start, End);
}

void Parser::LevitationOnParseStart() {
  if (!Tok.is(tok::kw___levitation_global))
    LevitationEnterUnit();
}

void Parser::LevitationOnParseEnd() {
  if (!LevitationUnitScopes.empty())
    LevitationLeaveUnit();
}

bool Parser::ParseLevitationGlobal() {

  BalancedDelimiterTracker T(*this, tok::l_brace);
  if (T.consumeOpen()) {
    Diag(Tok, diag::err_expected) << tok::l_brace;
    return false;
  }

  if (!LevitationUnitScopes.empty())
    LevitationLeaveUnit(Tok.getLocation(), Tok.getEndLoc());

  while (!tryParseMisplacedModuleImport() && Tok.isNot(tok::r_brace) &&
         Tok.isNot(tok::eof)) {
    ParsedAttributesWithRange attrs(AttrFactory);
    MaybeParseCXX11Attributes(attrs);
    ParseExternalDeclaration(attrs);
  }

  // The caller is what called check -- we are simply calling
  // the close for it.
  T.consumeClose();

  if (Tok.isNot(tok::eof)) {
    if (Tok.isNot(tok::kw___levitation_global))
      LevitationEnterUnit(Tok.getLocation(), Tok.getEndLoc());
    else
      Diag(Tok, diag::warn_levitation_two_sibling_globals);
  }

  return true;
}

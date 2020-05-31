//===---- ParseLevitation.cpp - Parser customization fir C++ Levitation ---===//
//
// Part of the C++ Levitation Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//===----------------------------------------------------------------------===//
//
//  This file implements additional parser methods for C++ Levitation mode.
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTConsumer.h"
#include "clang/Parse/Parser.h"
#include "clang/Levitation/UnitID.h"
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

static SourceLocation getNextLoc(const Sema &Actions) {
  auto AllFragments = Actions.levitationGetSourceFragments();
  auto &SM = Actions.getSourceManager();
  auto MainFileID = SM.getMainFileID();

  if (AllFragments.empty())
    return SM.getLocForStartOfFile(MainFileID);

  const auto &LastFragment = AllFragments.back();
  return SM.getLocForStartOfFile(MainFileID).getLocWithOffset((int)(LastFragment.End));
}

static SourceLocation getLastLoc(const Sema &Actions) {
  auto &SM = Actions.getSourceManager();
  auto MainFileID = SM.getMainFileID();
  return SM.getLocForEndOfFile(MainFileID);
}

/// By default whenever we parse C++ Levitation files,
/// we're in unit's namespace.
void Parser::LevitationEnterUnit(SourceLocation Start, SourceLocation End) {

  bool AtTUBounds = false;

  if (Start.isInvalid()) {
    AtTUBounds = true;
    Start = getNextLoc(Actions);
  }

  if (End.isInvalid())
    End = Start;

  StringRef UnitIDStr = getPreprocessor().getPreprocessorOpts().LevitationUnitID;

  if (LevitationUnitID.empty())
    UnitIDStr.split(
        LevitationUnitID,
        levitation::UnitIDUtils::getComponentSeparator()
    );

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

  Actions.levitationActOnEnterUnit(
      Start, End,
      LevitationUnitScopes.back().Namespace,
      AtTUBounds
  );
}

bool Parser::LevitationLeaveUnit(SourceLocation Start, SourceLocation End) {

  bool AtTUBounds = false;

  if (Start.isInvalid()) {
    AtTUBounds = true;
    Start = getLastLoc(Actions);
  }

  if (End.isInvalid())
    End = Start;

  if (LevitationUnitScopes.empty())
    llvm_unreachable("Unit Scope items info should not be empty.");

  SourceLocation LeaveUnitLoc = Tok.getLocation();

  NamespaceDecl *OuterNS = nullptr;

  // Leave scope in reverse order.
  while (!LevitationUnitScopes.empty()) {
    auto &ScopeItem = LevitationUnitScopes.back();
    OuterNS = ScopeItem.Namespace;

    ScopeItem.Scope->Exit();
    Actions.ActOnFinishNamespaceDef(ScopeItem.Namespace, LeaveUnitLoc);

    LevitationUnitScopes.pop_back();
  }

  assert(OuterNS && "Unit Scope items info should not be empty.");

  Actions.levitationActOnLeaveUnit(Start, End, AtTUBounds);

  return Actions.getASTConsumer().HandleTopLevelDecl(DeclGroupRef(OuterNS));
}

bool isAtLevitationGlobal(Parser *_this) {
  return
    _this->getCurToken().is(tok::kw_namespace) &&
    _this->NextToken().is(tok::coloncolon);
}

void Parser::LevitationOnParseStart() {
  Actions.ActOnStartOfTranslationUnit();
  if (!isAtLevitationGlobal(this))
    LevitationEnterUnit();
}

bool Parser::LevitationOnParseEnd() {
  Actions.ActOnEndOfTranslationUnit();
  if (!LevitationUnitScopes.empty())
    return LevitationLeaveUnit();
  return true;
}

/// ParseLevitationGlobal - we're about to parse global namespace declaration
///
///       global-scope-definition: [C++: namespace.def]
///         as-namespace-definition
///
///       as-namespace-definition:
///         'namespace' '::' '{' namespace-body '}'
///
bool Parser::ParseLevitationGlobal() {

  // Consume 'namespace'
  SourceLocation GlobalLoc = ConsumeToken();

  // Consume '::'
  ConsumeToken();

  SourceLocation LBraceEnd = Tok.getEndLoc();

  BalancedDelimiterTracker T(*this, tok::l_brace);
  if (T.consumeOpen()) {
    Diag(Tok, diag::err_expected) << tok::l_brace;
    return false;
  }

  if (!LevitationUnitScopes.empty()) {
    if (!LevitationLeaveUnit(GlobalLoc, LBraceEnd))
      return false;
  } else
    Actions.levitationAddSourceFragmentAction(
        GlobalLoc, LBraceEnd,
        levitation::SourceFragmentAction::SkipInHeaderOnly
    );

  auto &Consumer = Actions.getASTConsumer();
  Parser::DeclGroupPtrTy ADecl;

  // This part is similar to ParseAST top-level loop.
  // As long as we're in global, we should parse top-level declarations.
  while (Tok.isNot(tok::r_brace) && Tok.isNot(tok::eof)) {
    ParseTopLevelDecl(ADecl);
    if (ADecl && !Consumer.HandleTopLevelDecl(ADecl.get()))
        return false;
  }

  // The caller is what called check -- we are simply calling
  // the close for it.
  SourceLocation RBraceStart = Tok.getLocation();
  SourceLocation RBraceEnd = Tok.getEndLoc();
  T.consumeClose();

  if (Tok.isNot(tok::eof) && !isAtLevitationGlobal(this)) {
      LevitationEnterUnit(RBraceStart, RBraceEnd);
      return true;
  }

  Actions.levitationAddSourceFragmentAction(
    RBraceStart, RBraceEnd, levitation::SourceFragmentAction::SkipInHeaderOnly
  );

  return true;
}

bool Parser::ParseLevitationTranslationUnit() {

  assert(Actions.isLevitationMode(
      LangOptions::LBSK_BuildDeclAST,
      LangOptions::LBSK_BuildObjectFile
  ));

  LevitationOnParseStart();

  // Parse until EOF token or error encountered.
  while (true) {
    switch (Tok.getKind()) {
      case tok::annot_pragma_unused:
        HandlePragmaUnused();
        break;

      case tok::eof:
        // Check whether -fmax-tokens= was reached.
        if (PP.getMaxTokens() != 0 && PP.getTokenCount() > PP.getMaxTokens()) {
          PP.Diag(Tok.getLocation(), diag::warn_max_tokens_total)
              << PP.getTokenCount() << PP.getMaxTokens();
          SourceLocation OverrideLoc = PP.getMaxTokensOverrideLoc();
          if (OverrideLoc.isValid()) {
            PP.Diag(OverrideLoc, diag::note_max_tokens_total_override);
          }
        }
        return LevitationOnParseEnd();

      default:
        {
          if (
            Tok.is(tok::kw_namespace) &&
            NextToken().is(tok::coloncolon)
          ) {
            if (!ParseLevitationGlobal())
              return false;
            break;
          }

          ParsedAttributesWithRange attrs(AttrFactory);
          MaybeParseCXX11Attributes(attrs);
          ParseExternalDeclaration(attrs);
        }
        break;
    }
  }
}

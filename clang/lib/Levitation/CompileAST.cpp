// TODO Levitation: Licensing

#include "clang/Levitation/CompileAST.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ExternalASTSource.h"
#include "clang/AST/Stmt.h"
#include "clang/Parse/ParseDiagnostic.h"
#include "clang/Parse/Parser.h"
#include "clang/Sema/CodeCompleteConsumer.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaConsumer.h"
#include "clang/Sema/TemplateInstCallback.h"
#include "llvm/Support/CrashRecoveryContext.h"
#include "llvm/Support/TimeProfiler.h"
#include <cstdio>
#include <memory>

using namespace clang;

// TODO Levitation: get rid of all parser calls and parser itself.
void clang::levitation::CompileAST(
    Sema &S,
    llvm::StringRef LevitationAST
) {
//  // TODO Levitation: add one more external source into external sources
//  //   for exactly this external source read package dependent decls and
//  //   run package instantiation.
//  //   Remove package instantiation from Sema.cpp
//
//  //  // Collect global stats on Decls/Stmts (until we have a module streamer).
//  //  if (PrintStats) {
//  //    Decl::EnableStatistics();
//  //    Stmt::EnableStatistics();
//  //  }
//
//  //  // Also turn on collection of stats inside of the Sema object.
//  //  bool OldCollectStats = PrintStats;
//  //  std::swap(OldCollectStats, S.CollectStats);
//
//  // Initialize the template instantiation observer chain.
//  // FIXME: See note on "finalize" below.
//  initialize(S.TemplateInstCallbacks, S);
//
//  ASTConsumer *Consumer = &S.getASTConsumer();
//
//  std::unique_ptr<Parser> ParseOP(
//      new Parser(S.getPreprocessor(), S, SkipFunctionBodies));
//  Parser &P = *ParseOP.get();
//
//  llvm::CrashRecoveryContextCleanupRegistrar<const void, ResetStackCleanup>
//      CleanupPrettyStack(llvm::SavePrettyStackState());
//  PrettyStackTraceParserEntry CrashInfo(P);
//
//  // Recover resources if we crash before exiting this method.
//  llvm::CrashRecoveryContextCleanupRegistrar<Parser>
//    CleanupParser(ParseOP.get());
//
//  S.getPreprocessor().EnterMainSourceFile();
//  ExternalASTSource *External = S.getASTContext().getExternalSource();
//  if (External)
//    External->StartTranslationUnit(Consumer);
//
//  // If a PCH through header is specified that does not have an include in
//  // the source, or a PCH is being created with #pragma hdrstop with nothing
//  // after the pragma, there won't be any tokens or a Lexer.
//  bool HaveLexer = S.getPreprocessor().getCurrentLexer();
//
//  if (HaveLexer) {
//    llvm::TimeTraceScope TimeScope("Frontend", StringRef(""));
//    P.Initialize();
//    Parser::DeclGroupPtrTy ADecl;
//
//    // We skip stage below:
//    //    for (bool AtEOF = P.ParseFirstTopLevelDecl(ADecl); !AtEOF;
//    //         AtEOF = P.ParseTopLevelDecl(ADecl)) {
//    //      // If we got a null return and something *was* parsed, ignore it.  This
//    //      // is due to a top-level semicolon, an action override, or a parse error
//    //      // skipping something.
//    //      if (ADecl && !Consumer->HandleTopLevelDecl(ADecl.get()))
//    //        return;
//    //    }
//  }
//
//  // Process any TopLevelDecls generated by #pragma weak.
//  for (Decl *D : S.WeakTopLevelDecls())
//    Consumer->HandleTopLevelDecl(DeclGroupRef(D));
//
//  Consumer->HandleTranslationUnit(S.getASTContext());
//
//  // Finalize the template instantiation observer chain.
//  // FIXME: This (and init.) should be done in the Sema class, but because
//  // Sema does not have a reliable "Finalize" function (it has a
//  // destructor, but it is not guaranteed to be called ("-disable-free")).
//  // So, do the initialization above and do the finalization here:
//  finalize(S.TemplateInstCallbacks, S);
//
//  //  std::swap(OldCollectStats, S.CollectStats);
//  //  if (PrintStats) {
//  //    llvm::errs() << "\nSTATISTICS:\n";
//  //    if (HaveLexer) P.getActions().PrintStats();
//  //    S.getASTContext().PrintStats();
//  //    Decl::PrintStats();
//  //    Stmt::PrintStats();
//  //    Consumer->PrintStats();
//  //  }
}
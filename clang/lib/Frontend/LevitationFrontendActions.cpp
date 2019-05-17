//==-- LevitationFrontendActions.cpp --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/Frontend/LevitationFrontendActions.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTDiagnostic.h"
#include "clang/AST/ASTImporter.h"
#include "clang/AST/ASTImporterLookupTable.h"
#include "clang/Basic/FileManager.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Frontend/LevitationASTConsumers.h"
#include "clang/Frontend/MultiplexConsumer.h"
#include "clang/Frontend/Utils.h"
#include "clang/Levitation/FileExtensions.h"
#include "clang/Levitation/WithOperator.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Sema/TemplateInstCallback.h"
#include "clang/Serialization/ASTReader.h"
#include "clang/Serialization/ASTWriter.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/YAMLTraits.h"
#include <memory>
#include <system_error>
#include <utility>

using namespace clang;

namespace {

typedef std::vector<std::unique_ptr<ASTConsumer>> ConsumersVector;

struct OutputFileHandle {
  std::unique_ptr<llvm::raw_pwrite_stream> OutputStream;
  std::string Path;

  static OutputFileHandle makeInvalid() {
    return {nullptr, ""};
  }

  bool isInvalid() const { return !(bool)OutputStream; };
};

// Cloned with some alterations from GeneratePCHAction.
OutputFileHandle CreateOutputFile(CompilerInstance &CI, StringRef InFile) {
  // Comment from original method:
  // We use createOutputFile here because this is exposed via libclang, and we
  // must disable the RemoveFileOnSignal behavior.
  // We use a temporary to avoid race conditions.

  StringRef Extension = "";
  SmallString<256> OutputPath(CI.getFrontendOpts().OutputFile);

  std::string OutputPathName, TempPathName;
  std::error_code EC;

  std::unique_ptr<raw_pwrite_stream> OS = CI.createOutputFile(
      OutputPath,
      EC,
      true,
      false,
      InFile,
      Extension,
      true,
      false,
      &OutputPathName,
      &TempPathName
  );

  if (!OS) {
    CI.getDiagnostics().Report(diag::err_fe_unable_to_open_output)
    << OutputPath
    << EC.message();
    return OutputFileHandle::makeInvalid();
  }

  CI.addOutputFile({
      (OutputPathName != "-") ? OutputPathName : "",
      TempPathName
  });

  return { std::move(OS), std::move(OutputPathName) };
}


// Cloned with some alterations from GeneratePCHAction::CreateASTConsumer.
// We don't consider RelocatablePCH as an option for Build AST stage.
static std::unique_ptr<ASTConsumer>
CreateGeneratePCHConsumer(
    CompilerInstance &CI,
    StringRef InFile
) {
  OutputFileHandle OutputFile = CreateOutputFile(CI, InFile);

  if (OutputFile.isInvalid())
    return nullptr;

  const auto &FrontendOpts = CI.getFrontendOpts();
  auto Buffer = std::make_shared<PCHBuffer>();
  std::vector<std::unique_ptr<ASTConsumer>> Consumers;

  Consumers.push_back(llvm::make_unique<PCHGenerator>(
      CI.getPreprocessor(),
      CI.getModuleCache(),
      OutputFile.Path,
      "",
      Buffer,
      FrontendOpts.ModuleFileExtensions,
      CI.getPreprocessorOpts().AllowPCHWithCompilerErrors,
      FrontendOpts.IncludeTimestamps,
      +CI.getLangOpts().CacheGeneratedPCH
  ));

  Consumers.push_back(CI.getPCHContainerWriter().CreatePCHContainerGenerator(
      CI,
      InFile,
      OutputFile.Path,
      std::move(OutputFile.OutputStream),
      Buffer
  ));

  return llvm::make_unique<MultiplexConsumer>(std::move(Consumers));
}

class MultiplexConsumerBuilder {

  bool Successful = true;
  std::vector<std::unique_ptr<ASTConsumer>> Consumers;

public:

  using BuilderTy = MultiplexConsumerBuilder;

  BuilderTy &addRequired(std::unique_ptr<ASTConsumer> &&Consumer) {
    if (Successful && Consumer)
      Consumers.push_back(std::move(Consumer));
    else
      Successful = false;
    return *this;
  }

  BuilderTy &addNotNull(std::unique_ptr<ASTConsumer> &&Consumer) {
    assert(Consumer && "Consumer must be non-null pointer");
    return addRequired(std::move(Consumer));
  }

  std::unique_ptr<MultiplexConsumer> done() {
    return Successful ?
        llvm::make_unique<MultiplexConsumer>(std::move(Consumers)) :
        nullptr;
  }
};

class LevitationModulesReader : public ASTReader {
  LevitationModulesReader(CompilerInstance &CompilerInst) :
    ASTReader(
      CompilerInst.getPreprocessor(),
      CompilerInst.getModuleCache(),
      &CompilerInst.getASTContext(),
      CompilerInst.getPCHContainerReader(),
      {}
    )
  {}

public:

  class LevitationModulesOpenedReader : public levitation::WithOperand {

    bool Moved = false;
    LevitationModulesReader &Reader;

  public:

    LevitationModulesOpenedReader(LevitationModulesReader &Reader) :
      Reader(Reader)
    {}

    LevitationModulesOpenedReader(LevitationModulesOpenedReader &&Src)
    : Reader(Src.Reader)
    {
      Src.Moved = true;
    }

    ~LevitationModulesOpenedReader() {
      if (!Moved)
        Reader.close();
    }

    void readDependency(StringRef Dependency) {
      Reader.readDependency(Dependency);
    }
    void readMainFile(StringRef MainFile) {
      Reader.readMainFile(MainFile);
    }
  };

  static LevitationModulesOpenedReader open(CompilerInstance &CI) {
    auto *Reader = new LevitationModulesReader(CI);

    IntrusiveRefCntPtr<ASTReader> ThisReader(Reader);
    CI.setExternalSemaSource(ThisReader);

    Reader->open();

    return LevitationModulesOpenedReader(*Reader);
  }

private:

  void open() {
    beginRead();
  }

  void close() {
    endRead();
  }

  void beginRead() {
    // Beginning part of ASTReader::ReadAST, before ReadASTCore
    llvm_unreachable("TODO Levitation");

    //  llvm::SaveAndRestore<SourceLocation>
    //    SetCurImportLocRAII(CurrentImportLoc, ImportLoc);
    //
    //  // Defer any pending actions until we get to the end of reading the AST file.
    //  Deserializing AnASTFile(this);
    //
    //  // Bump the generation number.
    //  unsigned PreviousGeneration = 0;
    //  if (ContextObj)
    //    PreviousGeneration = incrementGeneration(*ContextObj);
    //
    //  unsigned NumModules = ModuleMgr.size();
    //  SmallVector<ImportedModule, 4> Loaded;
  }

  void endRead() {
    // Beginning part of ASTReader::ReadAST, after ReadASTCore
    llvm_unreachable("TODO Levitation");

    //
    //  // Here comes stuff that we only do once the entire chain is loaded.
    //
    //  // Load the AST blocks of all of the modules that we loaded.
    //  for (SmallVectorImpl<ImportedModule>::iterator M = Loaded.begin(),
    //                                              MEnd = Loaded.end();
    //       M != MEnd; ++M) {
    //    ModuleFile &F = *M->Mod;
    //
    //    // Read the AST block.
    //    if (ASTReadResult Result = ReadASTBlock(F, ClientLoadCapabilities))
    //      return Result;
    //
    //    // Read the extension blocks.
    //    while (!SkipCursorToBlock(F.Stream, EXTENSION_BLOCK_ID)) {
    //      if (ASTReadResult Result = ReadExtensionBlock(F))
    //        return Result;
    //    }
    //
    //    // Once read, set the ModuleFile bit base offset and update the size in
    //    // bits of all files we've seen.
    //    F.GlobalBitOffset = TotalModulesSizeInBits;
    //    TotalModulesSizeInBits += F.SizeInBits;
    //    GlobalBitOffsetsMap.insert(std::make_pair(F.GlobalBitOffset, &F));
    //
    //    // Preload SLocEntries.
    //    for (unsigned I = 0, N = F.PreloadSLocEntries.size(); I != N; ++I) {
    //      int Index = int(F.PreloadSLocEntries[I] - 1) + F.SLocEntryBaseID;
    //      // Load it through the SourceManager and don't call ReadSLocEntry()
    //      // directly because the entry may have already been loaded in which case
    //      // calling ReadSLocEntry() directly would trigger an assertion in
    //      // SourceManager.
    //      SourceMgr.getLoadedSLocEntryByID(Index);
    //    }
    //
    //    // Map the original source file ID into the ID space of the current
    //    // compilation.
    //    if (F.OriginalSourceFileID.isValid()) {
    //      F.OriginalSourceFileID = FileID::get(
    //          F.SLocEntryBaseID + F.OriginalSourceFileID.getOpaqueValue() - 1);
    //    }
    //
    //    // Preload all the pending interesting identifiers by marking them out of
    //    // date.
    //    for (auto Offset : F.PreloadIdentifierOffsets) {
    //      const unsigned char *Data = reinterpret_cast<const unsigned char *>(
    //          F.IdentifierTableData + Offset);
    //
    //      ASTIdentifierLookupTrait Trait(*this, F);
    //      auto KeyDataLen = Trait.ReadKeyDataLength(Data);
    //      auto Key = Trait.ReadKey(Data, KeyDataLen.first);
    //      auto &II = PP.getIdentifierTable().getOwn(Key);
    //      II.setOutOfDate(true);
    //
    //      // Mark this identifier as being from an AST file so that we can track
    //      // whether we need to serialize it.
    //      markIdentifierFromAST(*this, II);
    //
    //      // Associate the ID with the identifier so that the writer can reuse it.
    //      auto ID = Trait.ReadIdentifierID(Data + KeyDataLen.first);
    //      SetIdentifierInfo(ID, &II);
    //    }
    //  }
    //
    //  // Setup the import locations and notify the module manager that we've
    //  // committed to these module files.
    //  for (SmallVectorImpl<ImportedModule>::iterator M = Loaded.begin(),
    //                                              MEnd = Loaded.end();
    //       M != MEnd; ++M) {
    //    ModuleFile &F = *M->Mod;
    //
    //    ModuleMgr.moduleFileAccepted(&F);
    //
    //    // Set the import location.
    //    F.DirectImportLoc = ImportLoc;
    //    // FIXME: We assume that locations from PCH / preamble do not need
    //    // any translation.
    //    if (!M->ImportedBy)
    //      F.ImportLoc = M->ImportLoc;
    //    else
    //      F.ImportLoc = TranslateSourceLocation(*M->ImportedBy, M->ImportLoc);
    //  }
    //
    //  if (!PP.getLangOpts().CPlusPlus ||
    //      (Type != MK_ImplicitModule && Type != MK_ExplicitModule &&
    //       Type != MK_PrebuiltModule)) {
    //    // Mark all of the identifiers in the identifier table as being out of date,
    //    // so that various accessors know to check the loaded modules when the
    //    // identifier is used.
    //    //
    //    // For C++ modules, we don't need information on many identifiers (just
    //    // those that provide macros or are poisoned), so we mark all of
    //    // the interesting ones via PreloadIdentifierOffsets.
    //    for (IdentifierTable::iterator Id = PP.getIdentifierTable().begin(),
    //                                IdEnd = PP.getIdentifierTable().end();
    //         Id != IdEnd; ++Id)
    //      Id->second->setOutOfDate(true);
    //  }
    //  // Mark selectors as out of date.
    //  for (auto Sel : SelectorGeneration)
    //    SelectorOutOfDate[Sel.first] = true;
    //
    //  // Resolve any unresolved module exports.
    //  for (unsigned I = 0, N = UnresolvedModuleRefs.size(); I != N; ++I) {
    //    UnresolvedModuleRef &Unresolved = UnresolvedModuleRefs[I];
    //    SubmoduleID GlobalID = getGlobalSubmoduleID(*Unresolved.File,Unresolved.ID);
    //    Module *ResolvedMod = getSubmodule(GlobalID);
    //
    //    switch (Unresolved.Kind) {
    //    case UnresolvedModuleRef::Conflict:
    //      if (ResolvedMod) {
    //        Module::Conflict Conflict;
    //        Conflict.Other = ResolvedMod;
    //        Conflict.Message = Unresolved.String.str();
    //        Unresolved.Mod->Conflicts.push_back(Conflict);
    //      }
    //      continue;
    //
    //    case UnresolvedModuleRef::Import:
    //      if (ResolvedMod)
    //        Unresolved.Mod->Imports.insert(ResolvedMod);
    //      continue;
    //
    //    case UnresolvedModuleRef::Export:
    //      if (ResolvedMod || Unresolved.IsWildcard)
    //        Unresolved.Mod->Exports.push_back(
    //          Module::ExportDecl(ResolvedMod, Unresolved.IsWildcard));
    //      continue;
    //    }
    //  }
    //  UnresolvedModuleRefs.clear();
    //
    //  if (Imported)
    //    Imported->append(ImportedModules.begin(),
    //                     ImportedModules.end());
    //
    //  // FIXME: How do we load the 'use'd modules? They may not be submodules.
    //  // Might be unnecessary as use declarations are only used to build the
    //  // module itself.
    //
    //  if (ContextObj)
    //    InitializeContext();
    //
    //  if (SemaObj)
    //    UpdateSema();
    //
    //  if (DeserializationListener)
    //    DeserializationListener->ReaderInitialized(this);
    //
    //  ModuleFile &PrimaryModule = ModuleMgr.getPrimaryModule();
    //  if (PrimaryModule.OriginalSourceFileID.isValid()) {
    //    // If this AST file is a precompiled preamble, then set the
    //    // preamble file ID of the source manager to the file source file
    //    // from which the preamble was built.
    //    if (Type == MK_Preamble) {
    //      SourceMgr.setPreambleFileID(PrimaryModule.OriginalSourceFileID);
    //    } else if (Type == MK_MainFile) {
    //      SourceMgr.setMainFileID(PrimaryModule.OriginalSourceFileID);
    //    }
    //  }
    //
    //  // For any Objective-C class definitions we have already loaded, make sure
    //  // that we load any additional categories.
    //  if (ContextObj) {
    //    for (unsigned I = 0, N = ObjCClassesLoaded.size(); I != N; ++I) {
    //      loadObjCCategories(ObjCClassesLoaded[I]->getGlobalID(),
    //                         ObjCClassesLoaded[I],
    //                         PreviousGeneration);
    //    }
    //  }
    //
    //  if (PP.getHeaderSearchInfo()
    //          .getHeaderSearchOpts()
    //          .ModulesValidateOncePerBuildSession) {
    //    // Now we are certain that the module and all modules it depends on are
    //    // up to date.  Create or update timestamp files for modules that are
    //    // located in the module cache (not for PCH files that could be anywhere
    //    // in the filesystem).
    //    for (unsigned I = 0, N = Loaded.size(); I != N; ++I) {
    //      ImportedModule &M = Loaded[I];
    //      if (M.Mod->Kind == MK_ImplicitModule) {
    //        updateModuleTimestamp(*M.Mod);
    //      }
    //    }
    //  }
    //
    //  return Success;
  }

  void readDependency(StringRef Dependency) {
    llvm_unreachable("TODO Levitation");

    // below should be part of commond readFile(...):
    //
    //  switch (ASTReadResult ReadResult =
    //              ReadASTCore(FileName, Type, ImportLoc,
    //                          /*ImportedBy=*/nullptr, Loaded, 0, 0,
    //                          ASTFileSignature(), ClientLoadCapabilities)) {
    //  case Failure:
    //  case Missing:
    //  case OutOfDate:
    //  case VersionMismatch:
    //  case ConfigurationMismatch:
    //  case HadErrors: {
    //    llvm::SmallPtrSet<ModuleFile *, 4> LoadedSet;
    //    for (const ImportedModule &IM : Loaded)
    //      LoadedSet.insert(IM.Mod);
    //
    //    ModuleMgr.removeModules(ModuleMgr.begin() + NumModules, LoadedSet,
    //                            PP.getLangOpts().Modules
    //                                ? &PP.getHeaderSearchInfo().getModuleMap()
    //                                : nullptr);
    //
    //    // If we find that any modules are unusable, the global index is going
    //    // to be out-of-date. Just remove it.
    //    GlobalIndex.reset();
    //    ModuleMgr.setGlobalIndex(nullptr);
    //    return ReadResult;
    //  }
    //  case Success:
    //    break;
    //  }

  }

  void readMainFile(StringRef MainFile) {
    llvm_unreachable("TODO Levitation");

    // readFile(...)
  }
};

} // end of anonymous namespace

std::unique_ptr<ASTConsumer> LevitationBuildASTAction::CreateASTConsumer(
    clang::CompilerInstance &CI,
    llvm::StringRef InFile
) {

  return MultiplexConsumerBuilder()
      .addNotNull(CreateParserPostProcessor())
      .addNotNull(CreateDependenciesASTProcessor(CI, InFile))
      .addRequired(CreateGeneratePCHConsumer(CI, InFile))
  .done();
}

void LevitationBuildObjectAction::ExecuteAction() {
  // ASTFrontendAction is used as source

  CompilerInstance &CI = getCompilerInstance();
  if (!CI.hasPreprocessor())
    llvm_unreachable("Only actions with preprocessor are supported.");

  // FIXME: Move the truncation aspect of this into Sema, we delayed this till
  // here so the source manager would be initialized.
  if (hasCodeCompletionSupport() &&
      !CI.getFrontendOpts().CodeCompletionAt.FileName.empty())
    CI.createCodeCompletionConsumer();

  // Use a code completion consumer?
  CodeCompleteConsumer *CompletionConsumer = nullptr;
  if (CI.hasCodeCompletionConsumer())
    CompletionConsumer = &CI.getCodeCompletionConsumer();

  if (!CI.hasSema())
    CI.createSema(getTranslationUnitKind(), CompletionConsumer);

  importASTFiles();

  AdaptedAction->ExecuteAction();
  CI.getDiagnostics().getClient()->EndSourceFile();
}

void LevitationBuildObjectAction::importASTFiles() {

  CompilerInstance &CI = getCompilerInstance();

  CI.getDiagnostics().getClient()->BeginSourceFile(
      CI.getASTContext().getLangOpts()
  );

  CI.getDiagnostics().SetArgToStringFn(
      &FormatASTNodeDiagnosticArgument,
      &CI.getASTContext()
  );

  IntrusiveRefCntPtr<DiagnosticIDs> DiagIDs(
      CI.getDiagnostics().getDiagnosticIDs()
  );

  ASTImporterLookupTable LookupTable(
      *CI.getASTContext().getTranslationUnitDecl()
  );

  with (auto ReaderScope = LevitationModulesReader::open(CI)) {

    // Import dependencies
    for (const auto &Dep : ASTFiles) {
      ReaderScope.readDependency(Dep);
    }

    // Read precompiled main file if any.
    if (getCurrentFileKind().getFormat() == InputKind::Precompiled) {
      ReaderScope.readMainFile(getCurrentFile());
    }
  }
}

void LevitationBuildObjectAction::importAST(
    StringRef ASTFile,
    ASTImporterLookupTable &LookupTable,
    IntrusiveRefCntPtr<DiagnosticIDs> DiagIDs
) {
  // ASTMerge::ExecuteAction has been used as a source,
  // a bit refactored though...

  CompilerInstance &CI = getCompilerInstance();

  IntrusiveRefCntPtr<DiagnosticsEngine> Diags(new DiagnosticsEngine(
      DiagIDs,
      &CI.getDiagnosticOpts(),
      new ForwardingDiagnosticConsumer(*CI.getDiagnostics().getClient()),
      /*ShouldOwnClient=*/true
  ));

  std::unique_ptr<ASTUnit> Unit = ASTUnit::LoadFromASTFile(
      ASTFile,
      CI.getPCHContainerReader(),
      ASTUnit::LoadEverything,
      Diags,
      CI.getFileSystemOpts(),
      /*UseDebugInfo=*/false,
      /*OnlyLocalDecls=*/false,
      /*RemappedFiles=*/None,
      /*CaptureDiagnostics=*/false,
      /*AllowPCHWithCompilerErrors=*/false,
      /*UserFilesAreVolatile=*/false,
      /*ReadDeclarationsOnly=*/true
  );

  if (!Unit)
    return;

  ASTImporter Importer(
      CI.getASTContext(),
      CI.getFileManager(),
      Unit->getASTContext(),
      Unit->getFileManager(),
      /*MinimalImport=*/false,
      &LookupTable
  );

  TranslationUnitDecl *TU = Unit->getASTContext().getTranslationUnitDecl();

  importTU(Importer, TU);
}

void LevitationBuildObjectAction::importTU(
    ASTImporter& Importer, TranslationUnitDecl *TU
) {

  CompilerInstance &CI = getCompilerInstance();

  for (auto *D : TU->decls()) {
    // Don't re-import __va_list_tag, __builtin_va_list.
    if (const auto *ND = dyn_cast<NamedDecl>(D))
      if (IdentifierInfo *II = ND->getIdentifier())
        if (II->isStr("__va_list_tag") || II->isStr("__builtin_va_list"))
          continue;

    llvm::Expected<Decl *> ToDOrError = Importer.Import_New(D);

    if (ToDOrError) {
      DeclGroupRef DGR(*ToDOrError);
      CI.getASTConsumer().HandleTopLevelDecl(DGR);
    } else {
      llvm::consumeError(ToDOrError.takeError());
    }
  }
}

void LevitationBuildObjectAction::loadMainFile(llvm::StringRef MainFile) {

  CompilerInstance &CI = getCompilerInstance();

  IntrusiveRefCntPtr<ASTReader> Reader(new ASTReader(
      CI.getPreprocessor(),
      CI.getModuleCache(),
      &CI.getASTContext(),
      CI.getPCHContainerReader(),
      {}
  ));

  // Attach the AST reader to the AST context as an external AST
  // source, so that declarations will be deserialized from the
  // AST file as needed.
  // We need the external source to be set up before we read the AST, because
  // eagerly-deserialized declarations may use it.
  CI.getASTContext().setExternalSource(Reader);

  switch (Reader->ReadAST(
      MainFile,
      serialization::MK_MainFile,
      SourceLocation(),
      ASTReader::ARR_None
  )) {
  case ASTReader::Success:
    CI.getPreprocessor().setPredefines(Reader->getSuggestedPredefines());
    return;

  case ASTReader::Failure:
  case ASTReader::Missing:
  case ASTReader::OutOfDate:
  case ASTReader::VersionMismatch:
  case ASTReader::ConfigurationMismatch:
  case ASTReader::HadErrors:
    CI.getDiagnostics().Report(diag::err_fe_unable_to_load_pch);
    CI.setExternalSemaSource(nullptr);
  }
}

std::unique_ptr<ASTConsumer> LevitationBuildObjectAction::CreateASTConsumer(CompilerInstance &CI, StringRef InFile) {

  auto AdoptedConsumer = ASTMergeAction::CreateASTConsumer(CI, InFile);

  if (getCurrentFileKind().getFormat() != InputKind::Precompiled)
    return AdoptedConsumer;

  return MultiplexConsumerBuilder()
      .addNotNull(CreatePackageInstantiator())
      .addRequired(std::move(AdoptedConsumer))
  .done();
}

// TODO Levitation: Licensing
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticFrontend.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Levitation/CompilerInstanceExts.h"
#include "clang/Levitation/FrontendActionExts.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Sema/MultiplexExternalSemaSource.h"
#include "clang/Sema/Sema.h"
#include "clang/Serialization/ASTReader.h"
#include "clang/Serialization/InMemoryModuleCache.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/CrashRecoveryContext.h"

#include <initializer_list>

using namespace clang;
using namespace clang::levitation;

namespace clang {
namespace levitation {

namespace {

// So far we have nothing to override in MultiplexExternalSemaSource.
class DependenciesSemaSource : public MultiplexExternalSemaSource {
  SmallVector<std::unique_ptr<ASTReader>, 32> DepReaders;
public:
  void addSource(std::unique_ptr<ASTReader> &&StandaloneReader) {
    MultiplexExternalSemaSource::addSource(*StandaloneReader);
    DepReaders.push_back(std::move(StandaloneReader));
  }
};

static ASTReader *createASTReader(
    Preprocessor &PP,
    InMemoryModuleCache &ModuleCache,
    ASTContext &Context,
    const PCHContainerReader &ContainerReader,
    StringRef ASTFile,
    ASTDeserializationListener *Listener
) {
  auto Reader = std::unique_ptr<ASTReader>(new ASTReader(
      PP,
      ModuleCache,
      &Context,
      ContainerReader,
      /*Extensions=*/{},
      /*isysroot=*/"",
      /*DisableValidation=*/true
  ));

  if (Listener)
    Reader->setDeserializationListener(Listener);

  switch (Reader->ReadAST(
      ASTFile,
      serialization::MK_PCH,
      SourceLocation(),
      ASTReader::ARR_None
  )) {
  case ASTReader::Success:
    // Set the predefines buffer as suggested by the PCH reader.
    PP.setPredefines(Reader->getSuggestedPredefines());
    return Reader.release();

  case ASTReader::Failure:
  case ASTReader::Missing:
  case ASTReader::OutOfDate:
  case ASTReader::VersionMismatch:
  case ASTReader::ConfigurationMismatch:
  case ASTReader::HadErrors:
    break;
  }
  return nullptr;
}

class ChainedIncludesSourceImpl : public ExternalSemaSource {
public:
  ChainedIncludesSourceImpl(std::vector<std::unique_ptr<CompilerInstance>> CIs)
      : CIs(std::move(CIs)) {}

protected:
  //===----------------------------------------------------------------------===//
  // ExternalASTSource interface.
  //===----------------------------------------------------------------------===//

  /// Return the amount of memory used by memory buffers, breaking down
  /// by heap-backed versus mmap'ed memory.
  void getMemoryBufferSizes(MemoryBufferSizes &sizes) const override {
    for (unsigned i = 0, e = CIs.size(); i != e; ++i) {
      if (const ExternalASTSource *eSrc =
          CIs[i]->getASTContext().getExternalSource()) {
        eSrc->getMemoryBufferSizes(sizes);
      }
    }
  }

private:
  std::vector<std::unique_ptr<CompilerInstance>> CIs;
};

} // end of anonymous namespace

IntrusiveRefCntPtr<DependenciesSemaSource> createDepsSourceInternal(
    Preprocessor &PP,
    InMemoryModuleCache &ModuleCache,
    ASTContext &Context,
    const PCHContainerReader &ContainerReader,
    const std::vector<std::string> &ExternalSources,
    ASTDeserializationListener *Listener = nullptr
) {
  if (ExternalSources.empty())
    return nullptr;

  auto createReader = [&] (DependenciesSemaSource &Sources, StringRef Source) -> bool {
    auto Reader = std::unique_ptr<ASTReader>(createASTReader(
        PP, ModuleCache, Context, ContainerReader, Source, Listener
    ));

    if (!Reader)
      return false;

    Sources.addSource(std::move(Reader));

    return true;
  };

  IntrusiveRefCntPtr<DependenciesSemaSource> source = new DependenciesSemaSource();

  if (ExternalSources.size() == 1) {
    if (!createReader(*source, ExternalSources.front()))
      return nullptr;
  } else {
    for (auto &ES : ExternalSources) {
      if (!createReader(*source, ES))
        return nullptr;
    }
  }

  return source;
}

bool FrontendActionExts::createDepsSource(
    CompilerInstance &CI
) {
  ASTDeserializationListener *Listener = CI.hasASTConsumer() ?
      CI.getASTConsumer().GetASTDeserializationListener() :
      nullptr;

  auto source = createDepsSourceInternal(
    CI.getPreprocessor(),
    CI.getModuleCache(),
    CI.getASTContext(),
    CI.getPCHContainerReader(),
    CI.getPreprocessorOpts().LevitationDependencyDeclASTs,
    Listener
  );

  if (!source)
    return false;

  CI.setModuleManager(static_cast<ASTReader *>(source.get()));
  CI.getASTContext().setExternalSource(source);

  return true;
}

/// Shamelessly cloned from ASTInfoCollector, from ASTUnit.cpp
/// Gathers information from ASTReader that will be used to initialize
/// a Preprocessor.
class ASTLevitationInfoCollector : public ASTReaderListener {
    // FIXME Levitation: Only __COUNTER__ is required.
  Preprocessor &PP;
  ASTContext *Context;
  HeaderSearchOptions &HSOpts;
  PreprocessorOptions &PPOpts;
  LangOptions &LangOpt;
  std::shared_ptr<TargetOptions> &TargetOpts;
  IntrusiveRefCntPtr<TargetInfo> &Target;
  unsigned &Counter;
  bool InitializedLanguage = false;

public:
  ASTLevitationInfoCollector(
    Preprocessor &PP,
    ASTContext *Context,
    HeaderSearchOptions &HSOpts,
    PreprocessorOptions &PPOpts,
    LangOptions &LangOpt,
    std::shared_ptr<TargetOptions> &TargetOpts,
    IntrusiveRefCntPtr<TargetInfo> &Target,
    unsigned &Counter
  ) :
    PP(PP),
    Context(Context),
    HSOpts(HSOpts),
    PPOpts(PPOpts),
    LangOpt(LangOpt),
    TargetOpts(TargetOpts),
    Target(Target),
    Counter(Counter)
  {}

//  bool ReadLanguageOptions(
//      const LangOptions &LangOpts,
//      bool Complain,
//      bool AllowCompatibleDifferences
//  ) override {
//    if (InitializedLanguage)
//      return false;
//
//    LangOpt = LangOpts;
//    InitializedLanguage = true;
//
//    updated();
//    return false;
//  }
//
//  bool ReadHeaderSearchOptions(
//      const HeaderSearchOptions &HSOpts,
//      StringRef SpecificModuleCachePath,
//      bool Complain
//  ) override {
//    this->HSOpts = HSOpts;
//    return false;
//  }
//
//  bool ReadPreprocessorOptions(
//      const PreprocessorOptions &PPOpts,
//      bool Complain,
//      std::string &SuggestedPredefines
//  ) override {
//    this->PPOpts = PPOpts;
//    return false;
//  }
//
//  bool ReadTargetOptions(
//      const TargetOptions &TargetOpts,
//      bool Complain,
//      bool AllowCompatibleDifferences
//  ) override {
//    // If we've already initialized the target, don't do it again.
//    if (Target)
//      return false;
//
//    this->TargetOpts = std::make_shared<TargetOptions>(TargetOpts);
//    Target =
//        TargetInfo::CreateTargetInfo(PP.getDiagnostics(), this->TargetOpts);
//
//    updated();
//    return false;
//  }

  void ReadCounter(
      const serialization::ModuleFile &M, unsigned Value
  ) override {
    Counter = Value;
  }

private:
  void updated() {
    if (!Target || !InitializedLanguage)
      return;

    // Inform the target of the language options.
    //
    // FIXME: We shouldn't need to do this, the target should be immutable once
    // created. This complexity should be lifted elsewhere.
    Target->adjust(LangOpt);

    // Initialize the preprocessor.
    PP.Initialize(*Target);

    if (!Context)
      return;

    // Initialize the ASTContext
    Context->InitBuiltinTypes(*Target);

    // Adjust printing policy based on language options.
    Context->setPrintingPolicy(PrintingPolicy(LangOpt));

    // We didn't have access to the comment options when the ASTContext was
    // constructed, so register them now.
    Context->getCommentCommandTraits().registerCommentOptions(
        LangOpt.CommentOpts);
  }
};

/*static*/
class ASTUnitExts {
public:
  /// C++ Levitation version of ASTUnit::LoadFromASTFile
  /// Some unused code has been stripped.
  /// Added dependencies external source creation.
  /// Added package instantiation.
  ///
  /// Original comment:
  ///
  /// Create a ASTUnit from an AST file.
  ///
  /// \param Filename - The AST file to load.
  ///
  /// \param PCHContainerRdr - The PCHContainerOperations to use for loading and
  /// creating modules.
  /// \param Diags - The diagnostics engine to use for reporting errors; its
  /// lifetime is expected to extend past that of the returned ASTUnit.
  ///
  /// \returns - The initialized ASTUnit or null if the AST failed to load.  //
  static std::unique_ptr<ASTUnit> LoadFromASTFile(
      const std::string &Filename,
      CompilerInstance &CI
  ) {
    IntrusiveRefCntPtr<DiagnosticsEngine> Diags(&CI.getDiagnostics());
    const auto &FileSystemOpts = CI.getFileSystemOpts();

    std::unique_ptr<ASTUnit> AST(new ASTUnit(true));

    const auto &PCHContainerRdr = CI.getPCHContainerReader();

    // Recover resources if we crash before exiting this method.
    llvm::CrashRecoveryContextCleanupRegistrar<ASTUnit>
      ASTUnitCleanup(AST.get());
    llvm::CrashRecoveryContextCleanupRegistrar<DiagnosticsEngine,
      llvm::CrashRecoveryContextReleaseRefCleanup<DiagnosticsEngine>>
      DiagCleanup(Diags.get());

    ASTUnit::ConfigureDiags(Diags, *AST, /*CaptureDiagnostics=*/false);

    AST->LangOpts = std::make_shared<LangOptions>();
    *AST->LangOpts = CI.getLangOpts();

    AST->TargetOpts = std::make_shared<TargetOptions>();
    *AST->TargetOpts = CI.getTargetOpts();

    TargetInfo &Target = CI.getTarget();
    AST->Target = IntrusiveRefCntPtr<TargetInfo>(&Target);

    AST->Diagnostics = Diags;
    IntrusiveRefCntPtr<llvm::vfs::FileSystem> VFS =
        llvm::vfs::getRealFileSystem();
    AST->FileMgr = new FileManager(FileSystemOpts, VFS);
    AST->SourceMgr = new SourceManager(AST->getDiagnostics(),
                                       AST->getFileManager());
    AST->ModuleCache = new InMemoryModuleCache;
    AST->HSOpts = std::make_shared<HeaderSearchOptions>();
    AST->HSOpts->ModuleFormat = PCHContainerRdr.getFormat();
    AST->HeaderInfo.reset(new HeaderSearch(AST->HSOpts,
                                           AST->getSourceManager(),
                                           AST->getDiagnostics(),
                                           AST->getLangOpts(),
                                           &Target));
    AST->PPOpts = std::make_shared<PreprocessorOptions>();
    *AST->PPOpts = CI.getPreprocessorOpts();

    // Gather Info for preprocessor construction later on.

    HeaderSearch &HeaderInfo = *AST->HeaderInfo;
    unsigned Counter;

    AST->PP = std::make_shared<Preprocessor>(
        AST->PPOpts, AST->getDiagnostics(), *AST->LangOpts,
        AST->getSourceManager(), HeaderInfo, AST->ModuleLoader,
        /*IILookup=*/nullptr,
        /*OwnsHeaderSearch=*/false);
    Preprocessor &AstPP = *AST->PP;
    AstPP.Initialize(Target, CI.getAuxTarget());

    // Load everything

    AST->Ctx = new ASTContext(*AST->LangOpts, AST->getSourceManager(),
                              AstPP.getIdentifierTable(), AstPP.getSelectorTable(),
                              AstPP.getBuiltinInfo());

    AST->Ctx->InitBuiltinTypes(
        Target,
        CI.getAuxTarget()
    );

    AST->Reader = new ASTReader(
        AstPP,
        *AST->ModuleCache,
        AST->Ctx.get(),
        PCHContainerRdr,
        {},
        /*isysroot=*/"",
        /*DisableValidation=*/false,
        /*AllowPCHWithCompilerErrors=*/false
    );

    AST->Reader->setListener(llvm::make_unique<ASTLevitationInfoCollector>(
        *AST->PP, AST->Ctx.get(), *AST->HSOpts, *AST->PPOpts, *AST->LangOpts,
        AST->TargetOpts, AST->Target, Counter));

    // Attach the AST reader to the AST context as an external AST
    // source, so that declarations will be deserialized from the
    // AST file as needed.
    // We need the external source to be set up before we read the AST, because
    // eagerly-deserialized declarations may use it.
    const auto &Deps = CI.getPreprocessorOpts().LevitationDependencyDeclASTs;

//    assert(CI.hasASTContext() && "AST Context is required by LoadFromASTFile call.");
//    assert(CI.hasPreprocessor() && "Preprocessor is required by LoadFromASTFile call.");
//
    auto &Context = *AST->Ctx;
    auto &PP = *AST->PP;
    auto &ModuleCache = *AST->ModuleCache;

    auto source = createDepsSourceInternal(
        PP,
        ModuleCache,
        Context,
        PCHContainerRdr,
        Deps
    );

    source->MultiplexExternalSemaSource::addSource(*AST->Reader);

    AST->Ctx->setExternalSource(source);

    switch (AST->Reader->ReadAST(Filename, serialization::MK_MainFile,
                            SourceLocation(), ASTReader::ARR_None)) {
    case ASTReader::Success:
      break;

    case ASTReader::Failure:
    case ASTReader::Missing:
    case ASTReader::OutOfDate:
    case ASTReader::VersionMismatch:
    case ASTReader::ConfigurationMismatch:
    case ASTReader::HadErrors:
      AST->getDiagnostics().Report(diag::err_fe_unable_to_load_pch);
      return nullptr;
    }

    AST->OriginalSourceFile = AST->Reader->getOriginalSourceFile();

    AstPP.setCounterValue(Counter);

    // Create an AST consumer, even though it isn't used.
    AST->Consumer.reset(new ASTConsumer);

    // Create a semantic analysis object and tell the AST reader about it.
    AST->TheSema.reset(new Sema(AstPP, *AST->Ctx, *AST->Consumer));
    AST->TheSema->Initialize();
    AST->TheSema->InstantiatePackageClasses();

    // Tell the diagnostic client that we have started a source file.
    AST->getDiagnostics().getClient()->BeginSourceFile(AstPP.getLangOpts(), &AstPP);

    return AST;
  }
};

bool FrontendActionExts::loadFromAST(
    CompilerInstance &CI,
    FrontendAction &FA,
    const FrontendInputFile &Input
) {
  assert(!FA.usesPreprocessorOnly() && "this case was handled above");
  assert(FA.hasASTFileSupport() &&
         "This action does not have AST file support!");

  StringRef InputFile = Input.getFile();

//  CI.createASTContext();
//  CI.createPreprocessor(FA.getTranslationUnitKind());

  std::unique_ptr<ASTUnit> AST = ASTUnitExts::LoadFromASTFile(InputFile, CI);

  if (!AST)
    return false;

  // Inform the diagnostic client we are processing a source file.
  CI.getDiagnosticClient().BeginSourceFile(CI.getLangOpts(), nullptr);

  // Set the shared objects, these are reset when we finish processing the
  // file, otherwise the CompilerInstance will happily destroy them.
  CI.setFileManager(&AST->getFileManager());
  CI.setSourceManager(&AST->getSourceManager());
  CI.setPreprocessor(AST->getPreprocessorPtr());
  Preprocessor &PP = CI.getPreprocessor();
  PP.getBuiltinInfo().initializeBuiltins(PP.getIdentifierTable(),
                                         PP.getLangOpts());
  CI.setASTContext(&AST->getASTContext());

  FA.setCurrentInput(Input, std::move(AST));

  // Initialize the action.
  if (!FA.BeginSourceFileAction(CI))
    return false;

  // Create the AST consumer.
  CI.setASTConsumer(FA.CreateWrappedASTConsumer(CI, InputFile));
  if (!CI.hasASTConsumer())
    return false;

  return true;
}

}
}

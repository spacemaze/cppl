// TODO Levitation: Licensing
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Serialization/ASTReader.h"
#include "clang/Levitation/CompilerInstanceLevitation.h"
#include "clang/Levitation/DependenciesSemaSource.h"
#include "clang/Lex/Preprocessor.h"

#include "llvm/ADT/STLExtras.h"

using namespace clang;
using namespace clang::levitation;

namespace {

static ASTReader *createASTReader(
    CompilerInstance &CI,
    StringRef pchFile
) {
  Preprocessor &PP = CI.getPreprocessor();
  auto Reader = std::unique_ptr<ASTReader>(new ASTReader(
      PP,
      CI.getModuleCache(),
      &CI.getASTContext(),
      CI.getPCHContainerReader(),
      /*Extensions=*/{},
      /*isysroot=*/"",
      /*DisableValidation=*/true
  ));

  // FIXME Levitation: we should somehow get AST Consumer anyways
  if (CI.hasASTConsumer()) {
    Reader->setDeserializationListener(
        CI.getASTConsumer().GetASTDeserializationListener()
    );
  }

  switch (Reader->ReadAST(pchFile, serialization::MK_PCH, SourceLocation(),
                          ASTReader::ARR_None)) {
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

}

bool CompilerInvocationLevitation::createDependenciesSemaSource(
        clang::CompilerInstance &CI,
        const std::vector<std::string> &ExternalSources
) {
  if (!ExternalSources.size())
    llvm_unreachable("At least one source should be provided.");

  IntrusiveRefCntPtr<ExternalSemaSource> source;

  if (ExternalSources.size() == 1) {

      source = createASTReader(CI, ExternalSources.front());
      if (!source)
        return false;

  } else {

    DependenciesSemaSource *DepsSource = new DependenciesSemaSource();
    source = DepsSource;

    for (auto &ES : ExternalSources) {
      auto *ExternalSource = createASTReader(CI, ES);

      if (!ExternalSource)
        return false;

      DepsSource->addSource(*ExternalSource);
    }
  }

  CI.getASTContext().setExternalSource(source);
  return true;
}

InputKind CompilerInvocationLevitation::detectInputKind(
    clang::FrontendOptions &Opts,
    llvm::StringRef Input,
    InputKind OriginalInputKind
) {
    StringRef Extension = Input.rsplit('.').second;
    if (Extension == Opts.LevitationDeclASTFileExtension ||
        Extension == Opts.LevitationDefASTFileExtension) {
      return InputKind(
          OriginalInputKind.getLanguage(),
          InputKind::LevitationAST,
          OriginalInputKind.isPreprocessed()
      );
    }
}

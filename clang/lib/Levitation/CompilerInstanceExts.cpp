// TODO Levitation: Licensing
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Levitation/CompilerInstanceExts.h"
#include "clang/Levitation/FileExtensions.h"
#include "clang/Levitation/FrontendActionExts.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Serialization/ASTReader.h"

#include "llvm/ADT/STLExtras.h"

using namespace clang;
using namespace clang::levitation;

InputKind CompilerInvocationExts::detectInputKind(
    clang::FrontendOptions &Opts,
    llvm::StringRef Input,
    InputKind OriginalInputKind
) {
    StringRef Extension = Input.rsplit('.').second;

    if (
      Extension == FileExtensions::DeclarationAST ||
      Extension == FileExtensions::DefinitionAST
    ) {
      return InputKind(
          OriginalInputKind.getLanguage(),
          InputKind::LevitationAST,
          OriginalInputKind.isPreprocessed()
      );
    }
    return OriginalInputKind;
}

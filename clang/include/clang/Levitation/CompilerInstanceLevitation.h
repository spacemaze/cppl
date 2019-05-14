// TODO Levitation: Licensing

#ifndef LLVM_CLANG_LEVITATION_COMPILERINVOCATIONEXTENSIONS_H
#define LLVM_CLANG_LEVITATION_COMPILERINVOCATIONEXTENSIONS_H

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendOptions.h"
#include "llvm/ADT/StringRef.h"

#include <string>
#include <vector>

namespace clang {
namespace levitation {

  /*static*/
  class CompilerInvocationLevitation {
  public:
    static bool createDependenciesSemaSource(
        CompilerInstance &CI,
        const std::vector<std::string> &ExternalSources
    );
    static InputKind detectInputKind(
        FrontendOptions &Opts,
        StringRef Input,
        InputKind OriginalInputKind
    );
  };

}
}

#endif //LLVM_CLANG_LEVITATION_COMPILERINVOCATIONEXTENSIONS_H

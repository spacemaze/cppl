// TODO Levitation: Licensing

#ifndef LLVM_CLANG_LEVITATION_COMPILERINVOCATIONEXTENSIONS_H
#define LLVM_CLANG_LEVITATION_COMPILERINVOCATIONEXTENSIONS_H

#include "clang/Frontend/FrontendOptions.h"
#include "llvm/ADT/StringRef.h"

#include <string>
#include <vector>

namespace clang {
  class CompilerInstance;
  class FrontendAction;
namespace levitation {

  /*static*/
  class CompilerInvocationExts {
  public:
    static InputKind detectInputKind(
        FrontendOptions &Opts,
        StringRef Input,
        InputKind OriginalInputKind
    );
  };
}
}

#endif //LLVM_CLANG_LEVITATION_COMPILERINVOCATIONEXTENSIONS_H

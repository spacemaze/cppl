// TODO Levitation: Licensing

#ifndef LLVM_CLANG_LEVITATION_COMPILEASTLEVITATION_H
#define LLVM_CLANG_LEVITATION_COMPILEASTLEVITATION_H

#include "llvm/ADT/StringRef.h"

namespace clang {

class Sema;
namespace levitation {

  void CompileAST(Sema &S, llvm::StringRef LevitationAST);

}
}

#endif //LLVM_CLANG_LEVITATION_COMPILEASTLEVITATION_H

// TODO Levitation: Licensing

#ifndef LLVM_CLANG_LEVITATION_FILEEXTENSIONS_H
#define LLVM_CLANG_LEVITATION_FILEEXTENSIONS_H

namespace clang {
namespace levitation {

struct FileExtensions {
  static const char *SourceCode;
  static const char *DeclarationAST;
  static const char *DefinitionAST;
  static const char *DeclarationInstantiatedAST;
};

}
}

#endif //LLVM_CLANG_LEVITATION_FILEEXTENSIONS_H

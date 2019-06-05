// TODO Levitation: Licensing

#ifndef LLVM_CLANG_LEVITATION_FILEEXTENSIONS_H
#define LLVM_CLANG_LEVITATION_FILEEXTENSIONS_H

namespace clang {
namespace levitation {

struct FileExtensions {
  static constexpr char SourceCode [] = "cppl";
  static constexpr char DeclarationAST [] = "decl-ast";
  static constexpr char DefinitionAST [] = "def-ast";
  static constexpr char DeclarationInstantiatedAST [] = "ast";
  static constexpr char DirectDependencies [] = "ldeps";
};

}
}

#endif //LLVM_CLANG_LEVITATION_FILEEXTENSIONS_H

// TODO Levitation: Licensing

#ifndef LLVM_CLANG_LEVITATION_FILEEXTENSIONS_H
#define LLVM_CLANG_LEVITATION_FILEEXTENSIONS_H

namespace clang {
namespace levitation {

struct FileExtensions {
  static constexpr char SourceCode [] = "cppl";
  static constexpr char Object [] = "o";
  static constexpr char DeclarationAST [] = "decl-ast";
  static constexpr char ParsedAST [] = "ast";
  static constexpr char ParsedDependencies [] = "ldeps";
  static constexpr char DirectDependencies [] = "d";
  static constexpr char FullDependencies [] = "fulld";
};

}
}

#endif //LLVM_CLANG_LEVITATION_FILEEXTENSIONS_H

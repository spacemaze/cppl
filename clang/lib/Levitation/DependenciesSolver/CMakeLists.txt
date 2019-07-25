set(LLVM_LINK_COMPONENTS
  Support
)

add_clang_library(
  clangLevitationDependenciesSolver

  DependenciesSolver.cpp

  LINK_LIBS
  clangAST
  clangBasic
  clangLex
  clangSema
  clangLevitation
)

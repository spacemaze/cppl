set(LLVM_LINK_COMPONENTS
  Support
)

add_clang_library(
  clangLevitationDriver

  Driver.cpp
  DriverDefaults.cpp

  LINK_LIBS
  clangAST
  clangBasic
  clangLex
  clangSema
  clangLevitation
  clangLevitationDependenciesSolver
)

set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_library(
  clangLevitation

  Dependencies.cpp
  DependenciesSolver.cpp
  FileExtensions.cpp
  Serialization.cpp

  LINK_LIBS
  clangAST
  clangBasic
  clangLex
  clangSema
)

add_subdirectory(Driver)

set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_library(
  clangLevitation

  FileExtensions.cpp
  Serialization.cpp

  LINK_LIBS
  clangAST
  clangBasic
  clangLex
  clangSema
)

add_subdirectory(Driver)
add_subdirectory(DependenciesSolver)

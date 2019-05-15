set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_library(clangLevitation
  CompileAST.cpp
  Dependencies.cpp
  FileExtensions.cpp
  Serialization.cpp

  LINK_LIBS
  clangAST
  clangBasic
  clangLex
  clangSema
  )

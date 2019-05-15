set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_library(clangLevitation
  CompileAST.cpp
  CompilerInstanceExts.cpp
  Dependencies.cpp
  FileExtensions.cpp
  FrontendActionExts.cpp
  Serialization.cpp

  LINK_LIBS
  clangAST
  clangBasic
  clangLex
  clangSema
  )

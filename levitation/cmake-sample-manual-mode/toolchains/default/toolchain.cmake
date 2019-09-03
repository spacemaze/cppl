# Common part

set(LEVITATION_SourceCode_EXTENSION "cppl")

set(OBJECT_FILE_EXTENSION "o")

set(PCH_EXTENSION "pch")
set(LEVITATION_Object_EXTENSION "${OBJECT_FILE_EXTENSION}")
set(LEVITATION_DeclarationAST_EXTENSION "decl-ast")
set(LEVITATION_ParsedAST_EXTENSION "ast")
set(LEVITATION_ParsedDependencies_EXTENSION "ldeps")
set(LEVITATION_DirectDependencies_EXTENSION "d")
set(LEVITATION_FullDependencies_EXTENSION "fulld")

set(LEVITATION_CC_FLAGS -cc1 -std=c++17 -stdlib=libstdc++)
set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} -stdlib=libstdc++)

# Stages, their names will be used as custom targets

set(LEVITATION_OTHERS_STAGE "levitation-others")
set(LEVITATION_PREAMBLE_STAGE "levitation-preamble")
set(LEVITATION_PARSE_STAGE "levitation-parse")
set(LEVITATION_SOLVE_DEPENDENCIES_STAGE "levitation-dependencies")
set(LEVITATION_INSTANTIATE_STAGE "levitation-instantiate")
set(LEVITATION_COMPILE_STAGE "levitation-compile")

# Main file.
set(LEVITATION_PREAMBLE_FILE "preamble.cpp")
set(LEVITATION_MAIN_FILE "main.cpp")

# Command line arguments for each stage

set(LEVITATION_PARSE_ARGS
  ${LEVITATION_CC_FLAGS} -xc++ -levitation-build-ast
  -levitation-sources-root-dir=${CMAKE_SOURCE_DIR}
)

set(LEVITATION_DEPS_ARGS
  -deps-root=${PROJECT_BINARY_DIR}
)

set(LEVITATION_INSTANTIATE_ARGS
  ${LEVITATION_CC_FLAGS} -flevitation-build-decl -emit-pch
)

set(LEVITATION_COMPILE_ARGS
  ${LEVITATION_CC_FLAGS} -flevitation-build-object -emit-obj
)

# Toolchain

set(CLANG
  /Users/stepan/projects/shared/toolchains/llvm.git.darwin-debug-x86_64/bin/clang
)

set(LEVITATION_DEPS
  /Users/stepan/projects/shared/toolchains/llvm.git.darwin-debug-x86_64/bin/levitation-deps
)

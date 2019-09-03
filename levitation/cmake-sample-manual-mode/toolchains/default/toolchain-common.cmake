set(LEVITATION_SourceCode_EXTENSION "cppl")

set(OBJECT_FILE_EXTENSION "o")

set(LEVITATION_Object_EXTENSION "${OBJECT_FILE_EXTENSION}")
set(LEVITATION_DeclarationAST_EXTENSION "decl-ast")
set(LEVITATION_ParsedAST_EXTENSION "ast")
set(LEVITATION_ParsedDependencies_EXTENSION "ldeps")
set(LEVITATION_DirectDependencies_EXTENSION "d")
set(LEVITATION_FullDependencies_EXTENSION "fulld")

set(CC_FLAGS "-cc1 -std=c++17 -stdlib=libstdc++")
set(LINKER_FLAGS "-stdlib=libstdc++")

# BUILD_AST_FLAGS="-xc++ -levitation-build-ast -levitation-sources-root-dir=$PROJECT_DIR"

set(LEVITATION_PARSE_ARGS
  "${CC_FLAGS} -xc++ -levitation-build-ast "
  "-levitation-sources-root-dir=${CMAKE_SOURCE_DIR}"
)

set(LEVITATION_DEPS_ARGS
  "-deps-root=${PROJECT_BINARY_DIR}"
)

set(LEVITATION_INSTANTIATE_ARGS
  "${CC_FLAGS} -flevitation-build-decl -emit-pch"
)

set(LEVITATION_COMPILE_ARGS
  "${CC_FLAGS} -flevitation-build-object -emit-obj"
)
set(LEVITATION_DEPS_TARGET levitation-deps)

add_clang_executable(${LEVITATION_DEPS_TARGET} levitation-deps.cpp)

target_link_libraries(
    ${LEVITATION_DEPS_TARGET}
    clangLevitation
    clangAST
    clangBasic
    clangSerialization
)

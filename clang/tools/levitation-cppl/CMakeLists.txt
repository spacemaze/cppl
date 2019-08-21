set(LEVITATION_CPPL_TARGET cppl)

add_clang_tool(${LEVITATION_CPPL_TARGET} levitation-cppl.cpp)

target_link_libraries(
    ${LEVITATION_CPPL_TARGET}
    PRIVATE
    clangLevitationDriver
    clangAST
    clangBasic
    clangSerialization
)

set_target_properties(
    ${LEVITATION_CPPL_TARGET}
    PROPERTIES
    LINKER_LANGUAGE CXX
)

set(LEVITATION_DEPS_TARGET levitation-deps)

add_clang_executable(${LEVITATION_DEPS_TARGET} levitation-deps.cpp)

target_link_libraries(
    PRIVATE
    ${LEVITATION_DEPS_TARGET}
    clangLevitation
    clangAST
    clangBasic
    clangSerialization
)

set_target_properties(
    ${LEVITATION_DEPS_TARGET}
    PROPERTIES
    LINKER_LANGUAGE CXX
)

if (NOT LLVM_INSTALL_TOOLCHAIN_ONLY)
  if(INTERNAL_INSTALL_PREFIX)
    set(INSTALL_DESTINATION "${INTERNAL_INSTALL_PREFIX}/bin")
    set_property(TARGET ${LEVITATION_DEPS_TARGET} APPEND PROPERTY INSTALL_RPATH
       "@executable_path/../../lib")
  else()
    set(INSTALL_DESTINATION bin)
  endif()

  install(TARGETS ${LEVITATION_DEPS_TARGET}
    RUNTIME DESTINATION "${INSTALL_DESTINATION}"
    COMPONENT c-index-test)

  if (NOT LLVM_ENABLE_IDE)
    add_llvm_install_targets(install-${LEVITATION_DEPS_TARGET}
                             DEPENDS ${LEVITATION_DEPS_TARGET}
                             COMPONENT ${LEVITATION_DEPS_TARGET})
  endif()
endif()

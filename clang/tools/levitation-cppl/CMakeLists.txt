set(LEVITATION_CPPL_TARGET levitation-cppl)

add_clang_executable(${LEVITATION_CPPL_TARGET} levitation-cppl.cpp)

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

if (NOT LLVM_INSTALL_TOOLCHAIN_ONLY)
  if(INTERNAL_INSTALL_PREFIX)
    set(INSTALL_DESTINATION "${INTERNAL_INSTALL_PREFIX}/bin")
    set_property(TARGET ${LEVITATION_CPPL_TARGET} APPEND PROPERTY INSTALL_RPATH
       "@executable_path/../../lib")
  else()
    set(INSTALL_DESTINATION bin)
  endif()

  install(TARGETS ${LEVITATION_CPPL_TARGET}
    RUNTIME DESTINATION "${INSTALL_DESTINATION}"
    COMPONENT c-index-test)

  if (NOT LLVM_ENABLE_IDE)
    add_llvm_install_targets(install-${LEVITATION_CPPL_TARGET}
                             DEPENDS ${LEVITATION_CPPL_TARGET}
                             COMPONENT ${LEVITATION_CPPL_TARGET})
  endif()
endif()

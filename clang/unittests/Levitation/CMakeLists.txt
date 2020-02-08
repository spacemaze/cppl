set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Support
  )

add_clang_unittest(LevitationUnitTests
  LevitationUnitTests.cpp
)

clang_target_link_libraries(LevitationUnitTests
  PRIVATE
  clangLevitation
)

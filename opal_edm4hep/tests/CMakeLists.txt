# Unit tests that need no input file.
add_executable(helix_test helix_test.cpp)
target_link_libraries(helix_test PRIVATE opal_edm4hep)
add_test(NAME helix COMMAND helix_test)

# End-to-end tests need a converted OPAL ntuple; they are skipped when
# OPAL_TEST_INPUT is not set so that CI without /eos access still passes.
if(DEFINED ENV{OPAL_TEST_INPUT})
  add_test(NAME roundtrip
    COMMAND ${CMAKE_COMMAND}
      -DPASS=$<TARGET_FILE:opal_ntuple_pass>
      -DINPUT=$ENV{OPAL_TEST_INPUT}
      -DOUTDIR=${CMAKE_CURRENT_BINARY_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/roundtrip.cmake)
endif()

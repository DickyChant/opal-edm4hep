# Converts a small slice of a real ntuple and asserts the output is readable
# and non-empty. Driven by tests/CMakeLists.txt when OPAL_TEST_INPUT is set.
set(out "${OUTDIR}/roundtrip.edm4hep.root")
file(REMOVE "${out}")

execute_process(COMMAND "${PASS}" -n 200 "${INPUT}" "${out}"
                RESULT_VARIABLE rc OUTPUT_VARIABLE log ERROR_VARIABLE log)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "conversion failed:\n${log}")
endif()
if(NOT EXISTS "${out}")
  message(FATAL_ERROR "no output written")
endif()
file(SIZE "${out}" bytes)
if(bytes LESS 1000)
  message(FATAL_ERROR "output suspiciously small: ${bytes} bytes")
endif()
message(STATUS "roundtrip ok (${bytes} bytes)\n${log}")

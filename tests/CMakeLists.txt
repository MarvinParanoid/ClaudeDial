qt_add_executable(claudedial_tests
    tst_core.cpp
)

target_link_libraries(claudedial_tests PRIVATE claudedial_core Qt6::Test)

# The shipped SVG is read from the source tree, to check it against the
# canonical geometry it was generated from.
target_compile_definitions(claudedial_tests
    PRIVATE CLAUDEDIAL_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME core COMMAND claudedial_tests)

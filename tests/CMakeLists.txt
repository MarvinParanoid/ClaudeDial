qt_add_executable(claudometer_tests
    tst_core.cpp
)

target_link_libraries(claudometer_tests PRIVATE claudometer_core Qt6::Test)

# The shipped SVG is read from the source tree, to check it against the
# canonical geometry it was generated from.
target_compile_definitions(claudometer_tests
    PRIVATE CLAUDOMETER_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME core COMMAND claudometer_tests)

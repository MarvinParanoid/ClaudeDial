qt_add_executable(claudometer_tests
    tst_core.cpp
)

target_link_libraries(claudometer_tests PRIVATE claudometer_core Qt6::Test)

add_test(NAME core COMMAND claudometer_tests)

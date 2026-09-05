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

# The tray mark needs QtGui, which core deliberately does not link - so it gets
# its own executable rather than dragging a GUI dependency into the headless
# suite. The renderer's sources are compiled in directly; there is no library
# to link because the application is one target.
qt_add_executable(claudedial_render_tests
    tst_render.cpp
    ../src/GaugePainter.cpp
    ../src/tray/IconRenderer.cpp
)

target_link_libraries(claudedial_render_tests PRIVATE claudedial_core Qt6::Gui Qt6::Test)
target_include_directories(claudedial_render_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_test(NAME render COMMAND claudedial_render_tests)

# Offscreen, so it runs in a container and on a build machine with no display.
set_tests_properties(render PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

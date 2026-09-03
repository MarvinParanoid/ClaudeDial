cmake_minimum_required(VERSION 3.21)

project(claudometer
    VERSION 0.1.0
    DESCRIPTION "Claude Code usage at a glance"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(CLAUDOMETER_BUILD_TESTS "Build unit tests" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network DBus Gui Widgets Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.5)

add_compile_options(-Wall -Wextra -Wpedantic)

add_subdirectory(src/core)
add_subdirectory(src)

if(CLAUDOMETER_BUILD_TESTS)
    find_package(Qt6 6.5 QUIET COMPONENTS Test)
    if(Qt6Test_FOUND)
        enable_testing()
        add_subdirectory(tests)
    else()
        message(STATUS "Qt6Test not found - tests disabled")
    endif()
endif()

# --- installation -------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS claudometer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/claudometer.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(DIRECTORY data/icons/ DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons)

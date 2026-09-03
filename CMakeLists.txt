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

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network DBus Gui Widgets Quick QuickControls2 Svg)
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

# --- packaging ----------------------------------------------------------------
#
# Only three files are installed, and the QML lives inside the binary as a Qt
# resource, so every format packages the same small payload.

set(CPACK_PACKAGE_NAME claudometer)
set(CPACK_PACKAGE_VENDOR "Claudometer contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/OWNER/claudometer")
set(CPACK_PACKAGE_CONTACT "Claudometer contributors")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_FILE_NAME "claudometer-${PROJECT_VERSION}-linux-x86_64")
set(CPACK_STRIP_FILES ON)
set(CPACK_GENERATOR "TGZ")

# Let dpkg work the library dependencies out from the binary itself. Hard-coding
# them is a losing game: Debian and Ubuntu disagree on the Qt 6 package names,
# and Ubuntu's 64-bit time_t transition renamed them again.
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_SECTION utils)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

set(CPACK_RPM_PACKAGE_LICENSE MIT)
set(CPACK_RPM_PACKAGE_GROUP "Applications/System")
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase >= 6.5, qt6-qtdeclarative >= 6.5")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

include(CPack)

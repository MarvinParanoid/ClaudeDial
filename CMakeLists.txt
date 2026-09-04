cmake_minimum_required(VERSION 3.21)

project(claudedial
    VERSION 0.1.5
    DESCRIPTION "Claude Code usage at a glance"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(CLAUDEDIAL_BUILD_TESTS "Build unit tests" ON)

# 6.8, not 6.5, and the difference is not cosmetic. Declaring 6.5 while using
# newer APIs means a build on 6.5 fails deep in a translation unit with an
# unhelpful error instead of stopping at find_package with a clear one.
#
# What actually sets the floor:
#   QStyleHints::setColorScheme()   6.8  - the Light/Dark theme setting
#   QPalette::Accent                6.6  - following the desktop's accent colour
#   QStyleHints::colorScheme()      6.5
set(CLAUDEDIAL_QT_COMPONENTS Core Network Gui Widgets Quick QuickControls2)

# D-Bus is how a Linux desktop does notifications, suspend/resume and the tray
# host handshake. It is not a portability layer with alternatives behind it -
# elsewhere those three jobs have entirely different answers - so it is asked
# for only where it exists, and the code that uses it is compiled only there.
if (UNIX AND NOT APPLE)
    list(APPEND CLAUDEDIAL_QT_COMPONENTS DBus)
    set(CLAUDEDIAL_HAVE_DBUS ON)
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS ${CLAUDEDIAL_QT_COMPONENTS})
qt_standard_project_setup(REQUIRES 6.8)

if (MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(src/core)
add_subdirectory(src)

if(CLAUDEDIAL_BUILD_TESTS)
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

install(TARGETS claudedial RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# A .desktop entry and an XDG icon theme are meaningless anywhere else, and
# installing them would only make a Windows or macOS tree look wrong.
if (UNIX AND NOT APPLE)
    install(FILES data/claudedial.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
    install(DIRECTORY data/icons/ DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons)
endif()

# On Windows there is no system Qt to link against, so an install tree has to
# carry the runtime with it. Qt's own script rather than a hand-written
# windeployqt call: the first attempt at the latter deployed every DLL and the
# platform plugin and then no QML modules at all, and this is the path Qt
# supports for a QML application.
#
# --qmldir is still needed. The QML lives in the binary as a resource, so there
# is nothing for the import scanner to find unless it is pointed at the sources.
if (WIN32)
    qt_generate_deploy_qml_app_script(
        TARGET claudedial
        OUTPUT_SCRIPT CLAUDEDIAL_DEPLOY_SCRIPT
        NO_UNSUPPORTED_PLATFORM_ERROR
        DEPLOY_TOOL_OPTIONS --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/qml"
    )
    install(SCRIPT ${CLAUDEDIAL_DEPLOY_SCRIPT})
endif()

# --- packaging ----------------------------------------------------------------
#
# Only three files are installed, and the QML lives inside the binary as a Qt
# resource, so every format packages the same small payload.

set(CPACK_PACKAGE_NAME claudedial)
set(CPACK_PACKAGE_VENDOR "ClaudeDial contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/MarvinParanoid/ClaudeDial")
set(CPACK_PACKAGE_CONTACT "ClaudeDial contributors")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_FILE_NAME "claudedial-${PROJECT_VERSION}-linux-x86_64")
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

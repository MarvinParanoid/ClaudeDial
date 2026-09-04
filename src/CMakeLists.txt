qt_add_executable(claudedial
    AppIcon.cpp
    AppIcon.h
    Brand.h
    Application.cpp
    Application.h
    GaugePainter.cpp
    GaugePainter.h
    SingleInstance.cpp
    SingleInstance.h
    main.cpp
    cli/Cli.cpp
    cli/Cli.h
    tray/IconRenderer.cpp
    tray/IconRenderer.h
    tray/Notifier.cpp
    tray/Notifier.h
    tray/SleepWatcher.cpp
    tray/SleepWatcher.h
    tray/SystemTrayBackend.cpp
    tray/SystemTrayBackend.h
    tray/TrayBackend.h
    ui/Colors.cpp
    ui/Colors.h
    ui/GaugeItem.cpp
    ui/GaugeItem.h
    ui/PopupWindow.cpp
    ui/PopupWindow.h
    ui/SettingsViewModel.cpp
    ui/SettingsViewModel.h
    ui/UsageViewModel.cpp
    ui/UsageViewModel.h
)

# Set here, next to the target, and not in the top-level file after
# add_subdirectory: Qt finalises the target at the end of this directory, and a
# bundle property set later was ignored - the app got Qt's default Info.plist,
# without the LSUIElement that keeps a menu-bar utility out of the Dock.
#
# MACOSX_BUNDLE itself also has to be set before install(TARGETS) is called, or
# the target counts as RUNTIME and the bundle lands in bin/ where Qt's deploy
# tool cannot find it.
if (APPLE)
    set_target_properties(claudedial PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/../data/Info.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.marvinparanoid.claudedial"
        MACOSX_BUNDLE_BUNDLE_NAME "ClaudeDial"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    )
endif()

# Plain Qt resources rather than a QML module: this is one small app with five
# QML files, and the module machinery would only add moving parts.
qt_add_resources(claudedial "qml"
    PREFIX "/"
    BASE "ui"
    FILES
        ui/qml/IconButton.qml
        ui/qml/MeterRow.qml
        ui/qml/Popup.qml
        ui/qml/SectionHeader.qml
        ui/qml/Segmented.qml
        ui/qml/SettingRow.qml
        ui/qml/Stepper.qml
        ui/qml/Toggle.qml
        ui/qml/Settings.qml
        ui/qml/Theme.qml
)

# The application icon is one SVG, installed into the icon theme and embedded
# here so a build tree looks right too.
#
# Note what is *not* here: Qt6::Svg. Nothing calls QtSvg's API - QIcon
# rasterises the file through Qt's SVG *image-format plugin*, which is loaded at
# run time. So the qt6-svg package stays a dependency while the link line does
# not mention it, and listing it here only misled anyone reading this file.
qt_add_resources(claudedial "appicon"
    PREFIX "/icons"
    BASE "../data/icons/hicolor/scalable/apps"
    FILES ../data/icons/hicolor/scalable/apps/claudedial.svg
)

target_link_libraries(claudedial PRIVATE
    claudedial_core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
)

if (CLAUDEDIAL_HAVE_DBUS)
    target_link_libraries(claudedial PRIVATE Qt6::DBus)
    target_compile_definitions(claudedial PRIVATE CLAUDEDIAL_HAVE_DBUS)
endif()

target_include_directories(claudedial PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

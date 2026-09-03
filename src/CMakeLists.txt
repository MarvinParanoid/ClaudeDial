qt_add_executable(claudometer
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
    ui/GaugeItem.cpp
    ui/GaugeItem.h
    ui/PopupWindow.cpp
    ui/PopupWindow.h
    ui/SettingsViewModel.cpp
    ui/SettingsViewModel.h
    ui/UsageViewModel.cpp
    ui/UsageViewModel.h
)

# Plain Qt resources rather than a QML module: this is one small app with five
# QML files, and the module machinery would only add moving parts.
qt_add_resources(claudometer "qml"
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

target_link_libraries(claudometer PRIVATE
    claudometer_core
    Qt6::DBus
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
)

target_include_directories(claudometer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

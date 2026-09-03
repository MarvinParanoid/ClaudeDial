add_library(claudometer_core STATIC
    Config.cpp
    Config.h
    Credentials.cpp
    Credentials.h
    Format.cpp
    Format.h
    UsageClient.cpp
    UsageClient.h
    UsageJson.cpp
    UsageJson.h
    UsageLevel.cpp
    UsageLevel.h
    UsageService.cpp
    UsageService.h
    UsageState.h
)

# Core knows nothing about QML, QtQuick, widgets or D-Bus. Keeping the link
# line this short is what makes it testable headlessly.
target_link_libraries(claudometer_core PUBLIC Qt6::Core Qt6::Network)

target_include_directories(claudometer_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

target_compile_definitions(claudometer_core
    PUBLIC CLAUDOMETER_VERSION="${PROJECT_VERSION}"
)

set_target_properties(claudometer_core PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)

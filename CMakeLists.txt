cmake_minimum_required(VERSION 3.16)

project(Lumio VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Працює і з Qt 6, і з Qt 5.15
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network PrintSupport)

if(QT_VERSION_MAJOR EQUAL 5 AND Qt5_VERSION VERSION_LESS 5.15)
    message(FATAL_ERROR "Потрібен Qt 5.15 або новіший (знайдено ${Qt5_VERSION})")
endif()

set(SOURCES
    src/main.cpp
    src/app.h                src/app.cpp
    src/settings.h           src/settings.cpp
    src/imageio.h            src/imageio.cpp
    src/icons.h              src/icons.cpp
    src/annotation.h         src/annotation.cpp
    src/captureoverlay.h     src/captureoverlay.cpp
    src/pinwindow.h          src/pinwindow.cpp
    src/gallerywindow.h      src/gallerywindow.cpp
    src/settingsdialog.h     src/settingsdialog.cpp
    src/hotkeymanager.h      src/hotkeymanager.cpp
    src/platform.h           src/platform.cpp
    src/compat.h
    src/i18n.h               src/i18n.cpp
    src/updater.h            src/updater.cpp
)

if(WIN32)
    list(APPEND SOURCES resources/app.rc)
endif()

add_executable(Lumio ${SOURCES})

target_compile_definitions(Lumio PRIVATE APP_VERSION="${PROJECT_VERSION}")

target_link_libraries(Lumio PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::PrintSupport
)

if(WIN32)
    # щоб windres знайшов app.ico, на який посилається app.rc
    target_include_directories(Lumio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    target_link_libraries(Lumio PRIVATE user32 dwmapi)
    set_target_properties(Lumio PROPERTIES WIN32_EXECUTABLE ON)
endif()

if(MSVC)
    target_compile_options(Lumio PRIVATE /utf-8)
endif()

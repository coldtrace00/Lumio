# Альтернатива CMake для qmake / Qt Creator
QT += widgets network printsupport
CONFIG += c++17
TARGET = Lumio
TEMPLATE = app
VERSION = 1.0.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

SOURCES += \
    src/main.cpp \
    src/app.cpp \
    src/settings.cpp \
    src/imageio.cpp \
    src/icons.cpp \
    src/annotation.cpp \
    src/captureoverlay.cpp \
    src/pinwindow.cpp \
    src/gallerywindow.cpp \
    src/settingsdialog.cpp \
    src/hotkeymanager.cpp \
    src/platform.cpp \
    src/i18n.cpp

HEADERS += \
    src/app.h \
    src/settings.h \
    src/imageio.h \
    src/icons.h \
    src/annotation.h \
    src/captureoverlay.h \
    src/pinwindow.h \
    src/gallerywindow.h \
    src/settingsdialog.h \
    src/hotkeymanager.h \
    src/platform.h \
    src/compat.h \
    src/i18n.h

win32 {
    RC_ICONS = resources/app.ico
    LIBS += -luser32 -ldwmapi
}

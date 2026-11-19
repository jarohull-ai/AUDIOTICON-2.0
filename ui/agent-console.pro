# ASTRA UI template - agent console.
QT += core gui qml quick quickcontrols2 multimedia sql network
CONFIG += c++17 release
TARGET = astra-agent-console

INCLUDEPATH += src src/agent src/wallpaper src/doctor

include(src/core/core.pri)
include(bridge.pri)

SOURCES += \
    src/agent/main.cpp \
    src/agent/agentmodel.cpp \
    src/translator.cpp \
    src/wallpaper/WallpaperModel.cpp \
    src/doctor/DoctorModel.cpp

HEADERS += \
    src/agent/agentmodel.h \
    src/translator.h \
    src/wallpaper/WallpaperModel.h \
    src/doctor/DoctorModel.h

RESOURCES += src/resources.qrc

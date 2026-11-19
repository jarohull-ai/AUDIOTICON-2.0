# ASTRA UI template - agent console.
QT += core gui qml quick quickcontrols2 multimedia sql network
CONFIG += c++17 release
TARGET = astra-agent-console

INCLUDEPATH += src src/agent

include(src/core/core.pri)

SOURCES += \
    src/agent/main.cpp \
    src/agent/agentmodel.cpp \
    src/translator.cpp

HEADERS += \
    src/agent/agentmodel.h \
    src/translator.h

RESOURCES += src/resources.qrc

# ASTRA UI template - demo application.
QT += core gui qml quick quickcontrols2
CONFIG += c++17 release
TARGET = astra-ui-demo

INCLUDEPATH += src demo

SOURCES += \
    demo/main.cpp \
    src/dashboardmodel.cpp \
    src/translator.cpp

HEADERS += \
    src/dashboardmodel.h \
    src/translator.h \
    demo/demomodel.h

RESOURCES += src/resources.qrc

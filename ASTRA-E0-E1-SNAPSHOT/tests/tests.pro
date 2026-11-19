QT += core gui qml quick quickcontrols2 testlib
CONFIG += c++17 testcase
TARGET = tests

INCLUDEPATH += ../src ../src/agent

include(../src/core/core.pri)

SOURCES += tests.cpp ../src/dashboardmodel.cpp ../src/translator.cpp ../src/agent/agentmodel.cpp
HEADERS += ../src/dashboardmodel.h ../src/translator.h ../src/agent/agentmodel.h
RESOURCES += ../src/resources.qrc golden.qrc

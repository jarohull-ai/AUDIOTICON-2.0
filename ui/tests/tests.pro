QT += core gui qml quick quickcontrols2 testlib multimedia
CONFIG += c++17 testcase
TARGET = tests

INCLUDEPATH += ../src ../src/agent ../src/audio ../src/wallpaper ../src/doctor

include(../src/core/core.pri)

# M4: the test target links the Rust staticlib too, so the FFI link proof is a
# test case rather than a manual step. rustlink.pri also compiles the cxx façade.
include(../src/bridge/rustlink.pri)

# M5: the real StudioBridge (Rust job registry through the FFI) is under test, so
# it is built here instead of the M3 mock.
# M8: AudioPlayer (QAudioSink WAV playback) is under test.
# M10: WallpaperModel (Glass wallpaper system) is under test.
# M13: DoctorModel (Doctor diagnostics & FirstRunWizard) is under test.
SOURCES += tests.cpp ../src/dashboardmodel.cpp ../src/translator.cpp ../src/agent/agentmodel.cpp \
    ../src/bridge/StudioBridge.cpp ../src/bridge/BridgeObserver.cpp ../src/audio/AudioPlayer.cpp \
    ../src/wallpaper/WallpaperModel.cpp ../src/doctor/DoctorModel.cpp
HEADERS += ../src/dashboardmodel.h ../src/translator.h ../src/agent/agentmodel.h \
    ../src/bridge/StudioBridge.h ../src/bridge/BridgeObserver.h ../src/audio/AudioPlayer.h \
    ../src/wallpaper/WallpaperModel.h ../src/doctor/DoctorModel.h
RESOURCES += ../src/resources.qrc golden.qrc

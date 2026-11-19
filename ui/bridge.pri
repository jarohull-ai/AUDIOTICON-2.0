# bridge.pri - Qt <-> Rust integration surface.
#
# rustlink.pri does the actual linking (staticlib + cxx glue) and is included
# separately by the test target, which needs the link without the QObject façades.
include($$PWD/src/bridge/rustlink.pri)

INCLUDEPATH += $$PWD/src/bridge $$PWD/src/audio

HEADERS += \
    $$PWD/src/bridge/BridgeObserver.h \
    $$PWD/src/bridge/StudioBridge.h \
    $$PWD/src/audio/AudioPlayer.h

SOURCES += \
    $$PWD/src/bridge/BridgeObserver.cpp \
    $$PWD/src/bridge/StudioBridge.cpp \
    $$PWD/src/audio/AudioPlayer.cpp

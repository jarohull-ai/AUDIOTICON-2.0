# Audioticon core: shared by the GUI and the CLI, never linked against QML.
INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/Paths.h \
    $$PWD/voice/PolishVerbalizer.h \
    $$PWD/voice/DirectiveParser.h

SOURCES += \
    $$PWD/Paths.cpp \
    $$PWD/voice/PolishVerbalizer.cpp \
    $$PWD/voice/DirectiveParser.cpp

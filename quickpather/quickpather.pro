QT += quick

TARGET = quickpather
TEMPLATE = lib

DEFINES += QUICKPATHER_LIBRARY

HEADERS += \
    gametimer.h \
    quickentity.h \
    directpather.h \
    quickpather_global.h \
    gridpather.h \
    utils.h \
    gridpathnode.h \
    gridpathagent.h \
    steeringagent.h

SOURCES += \
    gametimer.cpp \
    quickentity.cpp \
    directpather.cpp \
    gridpather.cpp \
    gridpathnode.cpp \
    gridpathagent.cpp \
    steeringagent.cpp
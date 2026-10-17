QT += core gui qml quick quickcontrols2 network dbus

CONFIG += c++17 release
TARGET = needle
TEMPLATE = app

# One version for the package: ../VERSION (release-please bumps it)
NEEDLE_VERSION = $$cat($$PWD/../VERSION)
DEFINES += NEEDLE_VERSION=\\\"$$NEEDLE_VERSION\\\"

HEADERS += \
    src/config.h \
    src/engineclient.h \
    src/haystackclient.h \
    src/insight.h \
    src/outbox.h \
    src/playlistlibrary.h \
    src/playlists.h \
    src/prefs.h \
    src/review.h \
    src/reviewqueue.h \
    src/spectrumitem.h \
    src/theme.h

SOURCES += \
    src/main.cpp \
    src/config.cpp \
    src/engineclient.cpp \
    src/haystackclient.cpp \
    src/insight.cpp \
    src/outbox.cpp \
    src/playlistlibrary.cpp \
    src/playlists.cpp \
    src/prefs.cpp \
    src/review.cpp \
    src/reviewqueue.cpp \
    src/spectrumitem.cpp \
    src/theme.cpp

RESOURCES += src/resources.qrc

QT += core gui quick quickcontrols2 network dbus testlib
CONFIG += c++17 testcase
TARGET = needle_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/config.h ../src/engineclient.h ../src/haystackclient.h ../src/insight.h ../src/outbox.h \
           ../src/playlistlibrary.h ../src/playlists.h ../src/prefs.h ../src/review.h ../src/reviewqueue.h \
           ../src/theme.h
SOURCES += needle_tests.cpp ../src/config.cpp ../src/engineclient.cpp ../src/haystackclient.cpp ../src/insight.cpp \
           ../src/outbox.cpp ../src/playlistlibrary.cpp ../src/playlists.cpp ../src/prefs.cpp ../src/review.cpp \
           ../src/reviewqueue.cpp ../src/theme.cpp

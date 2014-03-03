QT       += testlib dbus
QT       -= gui

TARGET = tst_connectionagent
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

DEFINES += SRCDIR=\\\"$$PWD/\\\"

SOURCES += tst_connectionagent.cpp \
        ../../../connd/qconnectionmanager.cpp \
        ../../../connd/connadaptor.cpp \
        ../../../connd/wakeupwatcher.cpp

HEADERS += \
        ../../../connd/qconnectionmanager.h \
        ../../../connd/connadaptor.h \
        ../../../connd/wakeupwatcher.h

INCLUDEPATH += ../../../connd

CONFIG += link_pkgconfig
PKGCONFIG += connman-qt5 qofono-qt5


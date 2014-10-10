QT       += testlib dbus
QT       -= gui

TARGET = tst_connectionagent
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

DEFINES += SRCDIR=\\\"$$PWD/\\\"

SOURCES += tst_connectionagent.cpp \
        ../../../connd/qconnectionagent.cpp \
        ../../../connd/connectiond_adaptor.cpp \
        ../../../connd/wakeupwatcher.cpp
HEADERS += \
        ../../../connd/qconnectionagent.h \
        ../../../connd/connectiond_adaptor.h \
        ../../../connd/wakeupwatcher.h

INCLUDEPATH += $$OUT_PWD/../../../connd

CONFIG += link_pkgconfig
PKGCONFIG += connman-qt5 qofono-qt5


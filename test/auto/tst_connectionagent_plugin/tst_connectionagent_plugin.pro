#-------------------------------------------------
#
# Project created by QtCreator 2013-05-28T05:39:06
#
#-------------------------------------------------

QT       += testlib dbus network

QT       -= gui

TARGET = tst_connectionagent_plugintest
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += tst_connectionagent_plugintest.cpp \
        ../../../connectionagentplugin/declarativeconnectionagent.cpp

HEADERS += \
        ../../../connectionagentplugin/declarativeconnectionagent.h

DBUS_INTERFACES = connectiond_interface
connectiond_interface.files = ../../../connd/com.jollamobile.Connectiond.xml
connectiond_interface.header_flags = "-c ConnectionManagerInterface"
connectiond_interface.source_flags = "-c ConnectionManagerInterface"

DEFINES += SRCDIR=\\\"$$PWD/\\\"

INCLUDEPATH += ../../../connectiongentplugin

CONFIG += link_pkgconfig
PKGCONFIG += connman-qt5

target.path = $$[QT_INSTALL_PREFIX]/opt/tests/libqofono/
INSTALLS += target

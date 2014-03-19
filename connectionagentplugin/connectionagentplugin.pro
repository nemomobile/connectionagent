TEMPLATE = lib
TARGET = connectionagentplugin
QT = dbus qml
CONFIG += qt plugin

uri = com.jolla.connection

#create client
#system(qdbusxml2cpp ../connd/com.jollamobile.Connectiond.xml -c ConnectionManagerInterface -p connectionamanagerinterface)

SOURCES += \
    connectionagentplugin_plugin.cpp \
    connectionagentplugin.cpp \
    connectionamanagerinterface.cpp

HEADERS += \
    connectionagentplugin_plugin.h \
    connectionagentplugin.h \
    connectionamanagerinterface.h

OTHER_FILES = qmldir

MODULENAME = com/jolla/connection
TARGETPATH = $$[QT_INSTALL_QML]/$$MODULENAME

target.path = $$TARGETPATH
qmldir.files += qmldir
qmldir.path = $$TARGETPATH

INSTALLS += target qmldir

TEMPLATE = lib
TARGET = connectionagentplugin
QT += declarative dbus
CONFIG += qt plugin

uri = com.jolla.connection

#create client
#system(qdbusxml2cpp ../connd/com.jolla.Connectiond.xml -c ConnectionManagerInterface -p connectionamanagerinterface)

SOURCES += \
    connectionagentplugin_plugin.cpp \
    connectionagentplugin.cpp \
    connectionamanagerinterface.cpp

HEADERS += \
    connectionagentplugin_plugin.h \
    connectionagentplugin.h \
    connectionamanagerinterface.h

OTHER_FILES = qmldir

target.path = $$[QT_INSTALL_IMPORTS]/com/jolla/connection
qmldir.files += qmldir
qmldir.path = $$[QT_INSTALL_IMPORTS]/com/jolla/connection

INSTALLS += target qmldir

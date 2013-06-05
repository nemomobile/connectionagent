TEMPLATE = lib
TARGET = connectionagentplugin
QT += dbus
CONFIG += qt plugin

uri = com.jolla.connection

#create client
#system(qdbusxml2cpp ../connd/com.jollamobile.Connectiond.xml -c ConnectionManagerInterface -p connectionamanagerinterface)
equals(QT_MAJOR_VERSION, 4):  {
    QT += declarative
}
equals(QT_MAJOR_VERSION, 5):  {
    QT += qml
}
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
equals(QT_MAJOR_VERSION, 4): TARGETPATH = $$[QT_INSTALL_IMPORTS]/$$MODULENAME
equals(QT_MAJOR_VERSION, 5): TARGETPATH = $$[QT_INSTALL_QML]/$$MODULENAME

target.path = $$TARGETPATH
qmldir.files += qmldir
qmldir.path = $$TARGETPATH

INSTALLS += target qmldir

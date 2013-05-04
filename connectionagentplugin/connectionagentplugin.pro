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
    QT += quick
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

target.path = $$[QT_INSTALL_IMPORTS]/com/jolla/connection
qmldir.files += qmldir
qmldir.path = $$[QT_INSTALL_IMPORTS]/com/jolla/connection

INSTALLS += target qmldir

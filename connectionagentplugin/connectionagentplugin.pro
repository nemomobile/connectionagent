TEMPLATE = lib
TARGET = connectionagentplugin
QT = dbus qml
CONFIG += qt plugin

uri = com.jolla.connection

SOURCES += \
    plugin.cpp \
    declarativeconnectionagent.cpp

HEADERS += \
    declarativeconnectionagent.h

DBUS_INTERFACES = connectiond_interface
connectiond_interface.files = ../connd/com.jollamobile.Connectiond.xml
connectiond_interface.header_flags = "-c ConnectionManagerInterface"
connectiond_interface.source_flags = "-c ConnectionManagerInterface"

OTHER_FILES = qmldir

MODULENAME = com/jolla/connection
TARGETPATH = $$[QT_INSTALL_QML]/$$MODULENAME

target.path = $$TARGETPATH
qmldir.files += qmldir
qmldir.path = $$TARGETPATH

INSTALLS += target qmldir

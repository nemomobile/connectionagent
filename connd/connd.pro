
QT = core network dbus gui

TARGET = connectionagent
PKGCONFIG += connman-qt5 qofono-qt5

CONFIG   += console link_pkgconfig 
CONFIG   -= app_bundle

TEMPLATE = app

OTHER_FILES += com.jolla.Connectiond.xml

DBUS_ADAPTORS = connadaptor
connadaptor.files = com.jollamobile.Connectiond.xml
connadaptor.header_flags = -c ConnAdaptor
connadaptor.source_flags = -c ConnAdaptor

# create adaptor
#system(qdbusxml2cpp -c ConnAdaptor -a connadaptor.h:connadaptor.cpp com.jollamobile.Connectiond.xml)

SOURCES += main.cpp \
    qconnectionmanager.cpp \
    wakeupwatcher.cpp

HEADERS+= \
    qconnectionmanager.h \
    wakeupwatcher.h

target.path = /usr/bin
INSTALLS += target

MOC_DIR=.moc
OBJECTS_DIR=.obj




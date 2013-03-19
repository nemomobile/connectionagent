
QT += core network dbus gui
QT -= gui

TARGET = connectionagent
CONFIG   += console  
CONFIG   -= app_bundle

TEMPLATE = app

QT += core network dbus
QT -= gui


INCLUDEPATH += libconnman-qt

OTHER_FILES += com.jolla.Connectiond.xml

# create adaptor
#system(qdbusxml2cpp -c ConnAdaptor -a connadaptor.h:connadaptor.cpp com.jolla.Connectiond.xml)

SOURCES += main.cpp \
    qconnectionmanager.cpp \
    connadaptor.cpp


HEADERS+= \
    qconnectionmanager.h \
    connadaptor.h

LIBS += -lconnman-qt4
# LIBS += -llipstick

target.path = /usr/bin
INSTALLS += target

MOC_DIR=.moc
OBJECTS_DIR=.obj




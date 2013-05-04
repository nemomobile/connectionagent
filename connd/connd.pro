
QT += core network dbus gui
QT -= gui

equals(QT_MAJOR_VERSION, 4):  {
    TARGET = connectionagent
    INCLUDEPATH += libconnman-qt
    INCLUDEPATH += lipstick
    LIBS += -lconnman-qt4 -llipstick
}
equals(QT_MAJOR_VERSION, 5):  {
    TARGET = connectionagent
    INCLUDEPATH += libconnman-qt5
    INCLUDEPATH += lipstick
    LIBS += -lconnman-qt5 -llipstick-qt5
}

CONFIG   += console link_pkgconfig 
CONFIG   -= app_bundle

TEMPLATE = app

QT += core network dbus

OTHER_FILES += com.jolla.Connectiond.xml

# create adaptor
#system(qdbusxml2cpp -c ConnAdaptor -a connadaptor.h:connadaptor.cpp com.jollamobile.Connectiond.xml)

SOURCES += main.cpp \
    qconnectionmanager.cpp \
    connadaptor.cpp


HEADERS+= \
    qconnectionmanager.h \
    connadaptor.h

target.path = /usr/bin
INSTALLS += target

MOC_DIR=.moc
OBJECTS_DIR=.obj




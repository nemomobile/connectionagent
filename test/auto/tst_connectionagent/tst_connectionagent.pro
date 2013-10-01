QT       += testlib dbus
QT       -= gui

TARGET = tst_connectionagent
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

DEFINES += SRCDIR=\\\"$$PWD/\\\"

SOURCES += tst_connectionagent.cpp \
        ../../../connd/qconnectionmanager.cpp \
        ../../../connd/connadaptor.cpp

HEADERS += \
        ../../../connd/qconnectionmanager.h \
        ../../../connd/connadaptor.h

INCLUDEPATH += ../../../connd

equals(QT_MAJOR_VERSION, 4):  {
    PKGCONFIG += connman-qt4
    LIBS += -lconnman-qt4  -lqofono-qt
    INCLUDEPATH += /usr/include/connman-qt
}

equals(QT_MAJOR_VERSION, 5):  {
    PKGCONFIG += connman-qt5
    LIBS += -lconnman-qt5 -lqofono-qt5
    INCLUDEPATH += /usr/include/connman-qt5
}


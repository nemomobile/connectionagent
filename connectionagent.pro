TEMPLATE = subdirs

SUBDIRS += connectionagentplugin
SUBDIRS += test

equals(QT_MAJOR_VERSION, 4):  {
    SUBDIRS += test/testqml
    OTHER_FILES += rpm/connectionagent.spec }

equals(QT_MAJOR_VERSION, 5):  {
    SUBDIRS += config
    SUBDIRS += connd

    OTHER_FILES += rpm/connectionagent-qt5.spec \
                   rpm/connectionagent.tracing
}


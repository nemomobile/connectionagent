TEMPLATE = subdirs

SUBDIRS = \
    connectionagentplugin \
    test \
    config \
    connd

OTHER_FILES += rpm/connectionagent-qt5.spec \
               rpm/connectionagent-qt5.yaml \
               rpm/connectionagent.tracing


TEMPLATE = subdirs

SUBDIRS = \
    connectionagentplugin \
    test \
    config \
    connd \
    scripts

test.depends = connd # xml interface

OTHER_FILES += rpm/connectionagent-qt5.spec \
               rpm/connectionagent-qt5.yaml \
               rpm/connectionagent.tracing


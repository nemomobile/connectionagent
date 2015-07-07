TEMPLATE = subdirs

SUBDIRS = \
    connectionagentplugin \
    test \
    config \
    connd

test.depends = connd # xml interface

OTHER_FILES += rpm/connectionagent-qt5.spec

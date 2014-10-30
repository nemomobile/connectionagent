TEMPLATE = subdirs

OTHER_FILES += connectionagent-wrapper

wrapper.path = $${INSTALL_PREFIX}/usr/bin
wrapper.files = connectionagent-wrapper

INSTALLS += wrapper

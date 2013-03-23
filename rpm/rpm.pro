TEMPLATE = subdirs

OTHER_FILES += \
 com.jolla.Connectiond.service

service.path = $${INSTALL_PREFIX}/usr/share/dbus-1/services
service.files = com.jolla.Connectiond.service


INSTALLS += service

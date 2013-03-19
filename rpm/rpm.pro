OTHER_FILES += \
 com.jolla.Connectiond.service

service.path = $${INSTALL_PREFIX}/share/dbus-1/services
service.files = com.jolla.Connectiond.service


INSTALLS += service

TEMPLATE = subdirs

OTHER_FILES += \
 com.jolla.Connectiond.service \
    connectionagent.conf \
    connectionagent.service\
    connectionagent.spec\
    connectionagent.yaml

dbusservice.path = $${INSTALL_PREFIX}/usr/share/dbus-1/services
dbusservice.files = com.jolla.Connectiond.service

systemdservice.path = $${INSTALL_PREFIX}/usr/lib/systemd/user
systemdservice.files = connectionagent.service

INSTALLS += dbusservice systemdservice

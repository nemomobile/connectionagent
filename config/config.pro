TEMPLATE = subdirs

OTHER_FILES += \
    com.jolla.Connectiond.service \
    connectionagent.conf \
    connectionagent.service \
    70-connectionagent-tracing.conf

dbusservice.path = $${INSTALL_PREFIX}/usr/share/dbus-1/services
dbusservice.files = com.jolla.Connectiond.service

systemdservice.path = $${INSTALL_PREFIX}/usr/lib/systemd/user
systemdservice.files = connectionagent.service

dbusconfig.path = /etc/dbus-1/session.d
dbusconfig.files = connectionagent.conf

nemoenvironment.path = $${INSTALL_PREFIX}/var/lib/environment/nemo
nemoenvironment.files = 70-connectionagent-tracing.conf

INSTALLS += dbusservice systemdservice dbusconfig nemoenvironment

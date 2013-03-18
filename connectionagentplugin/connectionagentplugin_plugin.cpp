#include "connectionagentplugin_plugin.h"
#include "connectionagentplugin.h"

#include <qdeclarative.h>

void ConnectionagentpluginPlugin::registerTypes(const char *uri)
{
    qDebug() << Q_FUNC_INFO << uri;
    // @uri com.jolla.connection
    qmlRegisterType<ConnectionAgentPlugin>(uri, 1, 0, "ConnectionAgent");
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(Connectionagentplugin, ConnectionagentpluginPlugin)
#endif

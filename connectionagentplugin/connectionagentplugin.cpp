#include "connectionagentplugin.h"
#include "connectionamanagerinterface.h"

#include <qobject.h>

#define CONND_SERVICE "com.jolla.Connectiond"
#define CONND_PATH "/Connectiond"

ConnectionAgentPlugin::ConnectionAgentPlugin(QObject *parent):
    QObject(parent)
{
    qDebug() << Q_FUNC_INFO << "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX" << this;

    connectiondWatcher = new QDBusServiceWatcher(CONND_SERVICE,QDBusConnection::sessionBus(),
            QDBusServiceWatcher::WatchForRegistration |
            QDBusServiceWatcher::WatchForUnregistration, this);

    connect(connectiondWatcher, SIGNAL(serviceRegistered(QString)),
            this, SLOT(connectToConnectiond(QString)));
    connect(connectiondWatcher, SIGNAL(serviceUnregistered(QString)),
            this, SLOT(connectiondUnregistered(QString)));

    bool available = QDBusConnection::sessionBus().interface()->isServiceRegistered(CONND_SERVICE);

    if(available) {
        connectToConnectiond();
        qDebug() << Q_FUNC_INFO << CONND_SERVICE << "success!!";
    } else {
     qDebug() << Q_FUNC_INFO << CONND_SERVICE << "not available";
    }
}

ConnectionAgentPlugin::~ConnectionAgentPlugin()
{
}

void ConnectionAgentPlugin::sendUserReply(const QVariantMap &input)
{
    qDebug() << Q_FUNC_INFO << this << sender();
    QDBusPendingReply<> reply = connManagerInterface->sendUserReply(input);
    if (reply.isError()) {
     qDebug() << Q_FUNC_INFO << reply.error().message();
    }
}

void ConnectionAgentPlugin::sendConnectReply(const QString &replyMessage, int timeout)
{
    qDebug() << Q_FUNC_INFO << replyMessage << timeout;
     connManagerInterface->sendConnectReply(replyMessage,timeout);
}

void ConnectionAgentPlugin::onErrorReported(const QString &error)
{
    qDebug() << Q_FUNC_INFO << error;
    Q_EMIT errorReported(error);
}

void ConnectionAgentPlugin::onRequestBrowser(const QString &url)
{
    qDebug() << Q_FUNC_INFO <<url;
}

void ConnectionAgentPlugin::onUserInputRequested(const QString &service, const QVariantMap &fields)
{
    // we do this as qtdbus does not understand QVariantMap very well.
    // we need to manually demarshall
    QVariantMap map;
    QMapIterator<QString, QVariant> i(fields);
    // this works for Passphrase at least. anything else?
    while (i.hasNext()) {
        i.next();
        QDBusArgument arg = fields.value(i.key()).value<QDBusArgument>();
        QVariantMap vmap = qdbus_cast<QVariantMap>(arg);
        map.insert(i.key(), vmap);
    }
    Q_EMIT userInputRequested(service, map);
}

void ConnectionAgentPlugin::onConnectionRequested()
{
    qDebug() << Q_FUNC_INFO;
    Q_EMIT connectionRequest();
}

void ConnectionAgentPlugin::connectToConnectiond(QString)
{
    if (connManagerInterface) {
        delete connManagerInterface;
        connManagerInterface = 0;
    }
    connManagerInterface = new com::jolla::Connectiond(CONND_SERVICE, CONND_PATH, QDBusConnection::sessionBus(), this);

    connect(connManagerInterface,SIGNAL(connectionRequest()),
            this,SLOT(onConnectionRequested()));
    connect(connManagerInterface,SIGNAL(userInputCanceled()),
            this,SIGNAL(userInputCanceled()));

    connect(connManagerInterface,SIGNAL(errorReported(QString)),
                     this,SLOT(onErrorReported(QString)));

    connect(connManagerInterface,SIGNAL(requestBrowser(QString)),
                     this,SLOT(onRequestBrowser(QString)));

    connect(connManagerInterface,SIGNAL(userInputRequested(QString,QVariantMap)),
                     this,SLOT(onUserInputRequested(QString,QVariantMap)), Qt::UniqueConnection);
}

void ConnectionAgentPlugin::connectiondUnregistered(QString)
{
    if (connManagerInterface) {
        delete connManagerInterface;
        connManagerInterface = 0;
    }
}

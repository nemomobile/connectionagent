/****************************************************************************
**
** Copyright (C) 2013 Jolla Ltd
** Contact: lorn.potter@gmail.com
**
**
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
****************************************************************************/

#include "connectionagentplugin.h"
#include "connectionamanagerinterface.h"

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <connman-qt5/networkmanager.h>
#include <connman-qt5/networktechnology.h>
#include <connman-qt5/networkservice.h>
#else
#include <connman-qt/networkmanager.h>
#include <connman-qt/networktechnology.h>
#include <connman-qt/networkservice.h>
#endif

#include <qobject.h>

#define CONND_SERVICE "com.jolla.Connectiond"
#define CONND_PATH "/Connectiond"

ConnectionAgentPlugin::ConnectionAgentPlugin(QObject *parent):
    QObject(parent),
    connManagerInterface(0)
{
    connectiondWatcher = new QDBusServiceWatcher(CONND_SERVICE,QDBusConnection::sessionBus(),
            QDBusServiceWatcher::WatchForRegistration |
            QDBusServiceWatcher::WatchForUnregistration, this);

    connect(connectiondWatcher, SIGNAL(serviceRegistered(QString)),
            this, SLOT(connectToConnectiond(QString)));
    connect(connectiondWatcher, SIGNAL(serviceUnregistered(QString)),
            this, SLOT(connectiondUnregistered(QString)));

    connectToConnectiond();
}

ConnectionAgentPlugin::~ConnectionAgentPlugin()
{
}

void ConnectionAgentPlugin::connectToConnectiond(QString)
{
    qDebug() << Q_FUNC_INFO;

    if (connManagerInterface) {
        delete connManagerInterface;
        connManagerInterface = 0;
    }

    if (!QDBusConnection::sessionBus().interface()->isServiceRegistered(CONND_SERVICE)) {
        qDebug() << Q_FUNC_INFO << QString("connection service not available").arg(CONND_SERVICE);
        QDBusReply<void> reply = QDBusConnection::sessionBus().interface()->startService(CONND_SERVICE);

        if (!reply.isValid()) {
            qDebug() << Q_FUNC_INFO << reply.error().message();
            return;
        }
    }

    connManagerInterface = new com::jolla::Connectiond(CONND_SERVICE, CONND_PATH,
                                                       QDBusConnection::sessionBus(), this);
    if (!connManagerInterface->isValid()) {
        qDebug() << Q_FUNC_INFO << "is not valid interface";
    }
    connect(connManagerInterface,SIGNAL(connectionRequest()),
            this,SLOT(onConnectionRequested()));
    connect(connManagerInterface,SIGNAL(configurationNeeded(QString)),
            this,SIGNAL(configurationNeeded(QString)));

    connect(connManagerInterface,SIGNAL(userInputCanceled()),
            this,SIGNAL(userInputCanceled()));

    connect(connManagerInterface,SIGNAL(networkConnectivityUnavailable()),
            this,SIGNAL(networkConnectivityUnavailable()));
    connect(connManagerInterface,SIGNAL(networkConnectivityEstablished()),
            this,SIGNAL(networkConnectivityEstablished()));

    connect(connManagerInterface, SIGNAL(errorReported(QString, QString)),
            this, SLOT(onErrorReported(QString, QString)));

    connect(connManagerInterface,SIGNAL(connectionState(QString, QString)),
                     this,SLOT(onConnectionState(QString, QString)));

    connect(connManagerInterface,SIGNAL(requestBrowser(QString)),
                     this,SLOT(onRequestBrowser(QString)));

    connect(connManagerInterface,SIGNAL(userInputRequested(QString,QVariantMap)),
                     this,SLOT(onUserInputRequested(QString,QVariantMap)), Qt::UniqueConnection);
}

void ConnectionAgentPlugin::sendUserReply(const QVariantMap &input)
{    
    if (!connManagerInterface || !connManagerInterface->isValid()) {
        Q_EMIT errorReported("","ConnectionAgent not available");
        return;
    }
    QDBusPendingReply<> reply = connManagerInterface->sendUserReply(input);
    reply.waitForFinished();
    if (reply.isError()) {
        qDebug() << Q_FUNC_INFO << reply.error().message();
        Q_EMIT errorReported("",reply.error().message());
    }
}

void ConnectionAgentPlugin::sendConnectReply(const QString &replyMessage, int timeout)
{
    if (!connManagerInterface || !connManagerInterface->isValid()) {
        Q_EMIT errorReported("","ConnectionAgent not available");
        return;
    }
    connManagerInterface->sendConnectReply(replyMessage,timeout);
}

void ConnectionAgentPlugin::connectToType(const QString &type)
{
    if (!connManagerInterface || !connManagerInterface->isValid()) {
        Q_EMIT errorReported("","ConnectionAgent not available");
        return;
    }

    connManagerInterface->connectToType(type);
}

void ConnectionAgentPlugin::connectToService(const QString &servicePath)
{
    if (!connManagerInterface || !connManagerInterface->isValid()) {
        Q_EMIT errorReported("","ConnectionAgent not available");
        return;
    }

    connManagerInterface->connectToService(servicePath);
}

void ConnectionAgentPlugin::onErrorReported(const QString &servicePath, const QString &error)
{
    Q_EMIT errorReported(servicePath, error);
}

void ConnectionAgentPlugin::onRequestBrowser(const QString &url)
{
    qDebug() << Q_FUNC_INFO <<url;
    Q_EMIT browserRequested(url);
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

void ConnectionAgentPlugin::connectiondUnregistered(QString)
{
    if (connManagerInterface) {
        delete connManagerInterface;
        connManagerInterface = 0;
    }
}

void ConnectionAgentPlugin::onConnectionState(const QString &state, const QString &type)
{
    qDebug() << Q_FUNC_INFO << state;
    Q_EMIT connectionState(state, type);
}

bool ConnectionAgentPlugin::askRoaming() const
{
    return connManagerInterface->askRoaming();
}

void ConnectionAgentPlugin::setAskRoaming(bool value)
{
    connManagerInterface->setAskRoaming(value);
}


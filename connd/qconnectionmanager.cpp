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

#include "qconnectionmanager.h"
#include "connadaptor.h"

#include <connman-qt/useragent.h>
#include <connman-qt/service.h>

//#include <connman-qt/counter.h>
//#include <connman-qt/session.h>
//#include <connman-qt/networksession.h>
//#include <connman-qt/sessionagent.h>

#include <connman-qt/networkmanager.h>
#include <connman-qt/networktechnology.h>
#include <connman-qt/networkservice.h>

//#include <lipstick/notificationmanager.h>

QConnectionManager* QConnectionManager::self = NULL;

QConnectionManager::QConnectionManager(QObject *parent) :
    QObject(parent),
     netman(NetworkManagerFactory::createInstance()),
     netService(0),
     okToConnect(0),
     currentNetworkState(QString()),
     currentType(QString()),
     serviceConnect(0)
{
    connectionAdaptor = new ConnAdaptor(this);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    if (!dbus.registerObject("/Connectiond", this)) {
        qDebug() << "XXXXXXXXXXX could not register object XXXXXXXXXXXXXXXXXX";
    }
    if (!dbus.registerService("com.jolla.Connectiond")) {
        qDebug() << "XXXXXXXXXXX could not register service XXXXXXXXXXXXXXXXXX";
    }
    qDebug() << "XXXXXXXXXXX everything hunky dory XXXXXXXXXXXXXXXXXX";

    ua = new UserAgent(this);

    connect(ua,SIGNAL(userInputRequested(QString,QVariantMap)),
            this,SLOT(onUserInputRequested(QString,QVariantMap)));

    connect(ua,SIGNAL(connectionRequest()),this,SLOT(onConnectionRequest()));
    connect(ua,SIGNAL(errorReported(QString)),this,SLOT(onErrorReported(QString)));
    connect(ua,SIGNAL(userInputCanceled()),this,SLOT(onUserInputCanceled()));
    connect(ua,SIGNAL(userInputRequested(QString,QVariantMap)),
            this,SLOT(onUserInputRequested(QString,QVariantMap)), Qt::UniqueConnection);

    ua->sendConnectReply("Clear");

    connect(netman,SIGNAL(serviceAdded(QString)),this,SLOT(onServiceAdded(QString)));
    connect(netman,SIGNAL(stateChanged(QString)),this,SLOT(networkStateChanged(QString)));
    currentNetworkState = netman->state();
}

QConnectionManager::~QConnectionManager()
{
    delete self;
}

QConnectionManager & QConnectionManager::instance()
{
    if (!self) {
        self = new QConnectionManager;
    }

    return *self;
}

// from useragent
void QConnectionManager::onUserInputRequested(const QString &servicePath, const QVariantMap &fields)
{
    // gets called when a connman service gets called to connect and needs more configurations.

    Q_EMIT userInputRequested(servicePath, fields);
}

// from useragent
void QConnectionManager::onUserInputCanceled()
{
    Q_EMIT userInputCanceled();
}

// from useragent
void QConnectionManager::onErrorReported(const QString &error)
{
    qDebug() << Q_FUNC_INFO << error;
    Q_EMIT errorReported(error);
}

// from useragent
void QConnectionManager::onConnectionRequest()
{
    qDebug() << Q_FUNC_INFO;
    if (!autoConnect()) {
        Q_EMIT connectionRequest();
    }
}

void QConnectionManager::sendMessage()
{
  //  ua->sendConnectReply(QLatin1String("Suppress"));
}

void QConnectionManager::sendConnectReply(const QString &in0, int in1)
{
    ua->sendConnectReply(in0, in1);
}

void QConnectionManager::sendUserReply(const QVariantMap &input)
{
    qDebug() << Q_FUNC_INFO << input;
    ua->sendUserReply(input);
}

void QConnectionManager::networkStateChanged(const QString &state)
{
    if (serviceConnect)
        return;
    QString msg;
    if (currentNetworkState == "idle" && state == "ready") {
        msg = "Connecting...";
    } else  if (currentNetworkState == "ready" && state == "online") {
        msg = "Connected";
    } else  if (state == "offline") {
        msg = "Offline";
    } else if (state == "idle") {
        msg = "Disconnected";
    }
    qDebug() << Q_FUNC_INFO << currentNetworkState << state << msg;
    Q_EMIT connectionState(msg);

    //    if (!msg.isEmpty()) {
    //        emit serviceStateChanged(msg);
    //    }
    currentNetworkState = state;
}

void QConnectionManager::onServiceAdded(const QString &servicePath)
{
//    qDebug() << Q_FUNC_INFO << servicePath;
    if (okToConnect) {
        if (netService) {
            delete netService;
            netService = 0;
        }
        netService = new NetworkService(this);
        netService->setPath(servicePath);
        if (netService->favorite()) {
            serviceConnect = true;
            QObject::connect(netService, SIGNAL(stateChanged(QString)),
                             this,SLOT(stateChanged(QString)), Qt::UniqueConnection);
            QObject::connect(netService, SIGNAL(connectRequestFailed(QString)),
                             this,SLOT(serviceErrorChanged(QString)), Qt::UniqueConnection);

            netService->requestConnect();
        }
    }
}

void QConnectionManager::serviceErrorChanged(const QString &error)
{
    qDebug() << Q_FUNC_INFO << error;
    Q_EMIT errorReported(error);
}

void QConnectionManager::stateChanged(const QString &state)
{
    Q_UNUSED(state)
        QString msg;
        if ((currentNetworkState == "idle" && state == "association")
                || (currentNetworkState == "association" && state == "configuration")
                || (currentNetworkState == "configuration" && state == "ready")) {
            msg = "Connecting...";
        } else  if (currentNetworkState == "ready" && state == "online") {
            msg = "Connected";
            serviceConnect = false;
        } else if (state == "offline") {
            msg = "Offline";
        }
        qDebug() << Q_FUNC_INFO << currentNetworkState << state << msg;
        Q_EMIT connectionState(msg);
    //    if (!msg.isEmpty()) {
    //        emit serviceStateChanged(msg);
    //    }
        currentNetworkState = state;
}

bool QConnectionManager::autoConnect()
{
    QStringList techList = netman->technologiesList();
    Q_FOREACH (const QString &tech, techList) {

        QVector<NetworkService*> serviceList = netman->getServices(tech);

        Q_FOREACH (NetworkService *service, serviceList) {
            if(service->autoConnect() && service->favorite()) {
                serviceConnect = true;
                QObject::connect(service, SIGNAL(stateChanged(QString)),
                                 this,SLOT(stateChanged(QString)), Qt::UniqueConnection);
                QObject::connect(service, SIGNAL(connectRequestFailed(QString)),
                                 this,SLOT(serviceErrorChanged(QString)), Qt::UniqueConnection);

                service->requestConnect();
                return true;
            }
        }
    }
    return false;
}

void QConnectionManager::connectToType(const QString &type)
{
    qDebug() << Q_FUNC_INFO << type;
    currentType = type;
    QString techPath = netman->technologyPathForType(type);

    NetworkTechnology netTech;
    netTech.setPath(techPath);

    if (!netTech.powered()) {
        netTech.setPowered(true);
    }
    QStringList servicesList = netman->servicesList(type);

    qDebug() << servicesList;

    if (servicesList.isEmpty()) {
        if (type == "wifi") {
            QObject::connect(&netTech,SIGNAL(scanFinished()),this,SLOT(onScanFinished()));
            netTech.scan();
        } else {
            onScanFinished();
        }
    } else {
        if (netService) {
            delete netService;
            netService = 0;
        }
        netService = new NetworkService(this);
        bool needConfig = false;
        Q_FOREACH (const QString path, servicesList) {

            netService->setPath(path);

            if (netService->favorite()) {
                qDebug() << "power on, fav";
                needConfig = false;
                serviceConnect = true;
                QObject::connect(netService, SIGNAL(stateChanged(QString)),
                                 this,SLOT(stateChanged(QString)), Qt::UniqueConnection);

                QObject::connect(netService, SIGNAL(connectRequestFailed(QString)),
                                 this,SLOT(serviceErrorChanged(QString)), Qt::UniqueConnection);

                qDebug() << Q_FUNC_INFO << "just connect to this  thing";

//                NetworkTechnology *tech = netman->getTechnology(netService->type());
//                tech->setIdleTimeout(120);

                netService->requestConnect();
                okToConnect = false;
                return;
            } else {
                needConfig = true;
            }
        }
        if (needConfig) {
            qDebug() << Q_FUNC_INFO << "no favorite service found. Configuration needed";
            Q_EMIT wlanConfigurationNeeded();
        }
    }

}

void QConnectionManager::onScanFinished()
{
    qDebug() << Q_FUNC_INFO;
}

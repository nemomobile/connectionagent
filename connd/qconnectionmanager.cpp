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

#include <connman-qt/manager.h>

#include <connman-qt/networkmanager.h>
#include <connman-qt/networktechnology.h>
#include <connman-qt/networkservice.h>
#include <connman-qt/sessionagent.h>

//#include <lipstick/notificationmanager.h>

QConnectionManager* QConnectionManager::self = NULL;

#define CONND_SERVICE "com.jolla.Connectiond"
#define CONND_PATH "/Connectiond"


// TODO single connection

QConnectionManager::QConnectionManager(QObject *parent) :
    QObject(parent),
     netman(NetworkManagerFactory::createInstance()),
  //   netService(0),
     okToConnect(0),
     currentNetworkState(QString()),
     currentType(QString()),
     serviceConnect(0),
     currentNotification(0)
{
    bool available = QDBusConnection::sessionBus().interface()->isServiceRegistered(CONND_SERVICE);

    qDebug() << Q_FUNC_INFO << available;
    connectionAdaptor = new ConnAdaptor(this);
    QDBusConnection dbus = QDBusConnection::sessionBus();

    if (!dbus.registerService(CONND_SERVICE)/*,
            QDBusConnectionInterface::ReplaceExistingService,
            QDBusConnectionInterface::DontAllowReplacement*/) {
        qDebug() << "XXXXXXXXXXX could not register service XXXXXXXXXXXXXXXXXX";
    }

    if (!dbus.registerObject(CONND_PATH, this)) {
        qDebug() << "XXXXXXXXXXX could not register object XXXXXXXXXXXXXXXXXX";
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
    connect(netman,SIGNAL(serviceRemoved(QString)),this,SLOT(onServiceRemoved(QString)));

    connect(netman,SIGNAL(defaultRouteChanged(NetworkService*)),
            this,SLOT(defaultRouteChanged(NetworkService*)));

    updateServicesMap();
    currentNetworkState = netman->state();

    // let me control autoconnect
    netman->setSessionMode(true);

//    sessionAgent = new SessionAgent("/ConnectionSessionAgent",this);
//    sessionAgent->setConnectionType("internet");
//    sessionAgent->setAllowedBearers(QStringList() << "wifi" << "cellular" << "bluetooth");
//    connect(sessionAgent,SIGNAL(settingsUpdated(QVariantMap)),
//            this,SLOT(sessionSettingsUpdated(QVariantMap)));

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

void QConnectionManager::onServiceAdded(const QString &servicePath)
{
    //TODO connection migration?

    //    qDebug() << Q_FUNC_INFO << servicePath << okToConnect << currentType;
    //      qDebug() << "connected services" << connectedServices;

    if (!servicesMap.contains(servicePath)) {
        updateServicesMap();

        //        NetworkService *netService = new NetworkService(this);
        //        netService->setPath(servicePath);
        //        qDebug() << Q_FUNC_INFO << "add" << netService->name();
        ////TODO sort ?

        //        servicesMap.insert(servicePath, netService);

        //        QObject::connect(servicesMap.value(servicePath), SIGNAL(stateChanged(QString)),
        //                         this,SLOT(stateChanged(QString)), Qt::UniqueConnection);
    }


    if (okToConnect && !currentType.isEmpty()) {
        if (servicesMap.contains(servicePath)
                && servicesMap.value(servicePath)->type() == currentType
                && servicesMap.value(servicePath)->favorite()) {
            connectToNetworkService(servicePath);
        } else {
            Q_EMIT wlanConfigurationNeeded();
            serviceConnect = false;
        }
    }
}

void QConnectionManager::onServiceRemoved(const QString &servicePath)
{
    qDebug() << Q_FUNC_INFO << servicePath;
    updateServicesMap();
    qDebug() << servicesMap.keys();
}

void QConnectionManager::serviceErrorChanged(const QString &error)
{
    qDebug() << Q_FUNC_INFO << error;
    Q_EMIT errorReported(error);
}

void QConnectionManager::stateChanged(const QString &state)
{
    NetworkService *service = qobject_cast<NetworkService *>(sender());

    qDebug() << Q_FUNC_INFO << state << service->name();

//    qDebug() << Q_FUNC_INFO << currentNetworkState << state;
//    if (state == "online" || state == "association" || state == "idle"))

    if (!(currentNetworkState == "online" && state == "association"))
        Q_EMIT connectionState(state, service->type());

    currentNetworkState = state;
    qDebug() << Q_FUNC_INFO << state ;
}

bool QConnectionManager::autoConnect()
{
    qDebug() << Q_FUNC_INFO <<  servicesMap.keys() <<  servicesMap.keys().count();

    QString selectedService;
    uint strength = 0;
    QString currentType;

    Q_FOREACH (const QString &servicePath, servicesMap.keys()) {

        if(servicesMap.value(servicePath)->autoConnect()
                && servicesMap.value(servicePath)->favorite()) {

            qDebug() << servicesMap.value(servicePath)->name()
                     << servicesMap.value(servicePath)->strength();

            if (!selectedService.isEmpty()
                    && (!currentType.isEmpty()
                        && servicesMap.value(servicePath)->type() != currentType)) {
                qDebug() << "break here";
                break;
            }
            if ((servicesMap.value(servicePath)->strength() > strength)) {

                selectedService = servicePath;
                strength = servicesMap.value(servicePath)->strength();
            }
            currentType = servicesMap.value(servicePath)->type();

            //                sessionAgent->setAllowedBearers(QStringList() <<service->type());//<< "wifi" << "cellular");
            //                    sessionAgent->setAllowedBearers(QStringList() << "cellular" << "wifi");
            //              sessionAgent->requestConnect();

            //                QObject::connect(service, SIGNAL(stateChanged(QString)),
            //                                 this,SLOT(stateChanged(QString)), Qt::UniqueConnection);
            //                QObject::connect(service, SIGNAL(connectRequestFailed(QString)),
            //                                 this,SLOT(serviceErrorChanged(QString)), Qt::UniqueConnection);

        }
    }

    qDebug() << "out of loop now";
    if (!selectedService.isEmpty()) {
        qDebug() << Q_FUNC_INFO << selectedService;
        qDebug() << Q_FUNC_INFO << servicesMap.value(selectedService)->name();
        serviceConnect = true;
        servicesMap.value(selectedService)->requestConnect();
        return true;
    }

    qDebug() << Q_FUNC_INFO << false;
    return false;
}

void QConnectionManager::connectToType(const QString &type)
{
    qDebug() << Q_FUNC_INFO << type;
    currentType = type;
    QString techPath = netman->technologyPathForType(type);

    NetworkTechnology netTech;
    netTech.setPath(techPath);

    if (!netTech.powered()) { //?
        netTech.setPowered(true);
    }
    QStringList servicesList = netman->servicesList(type);

    qDebug() << servicesList;

    if (servicesList.isEmpty()) {
        if (type == "wifi") {
            okToConnect = true;
            QObject::connect(&netTech,SIGNAL(scanFinished()),this,SLOT(onScanFinished()));
            netTech.scan();
        } else {
            onScanFinished();
        }
    } else {
        currentType = "";
        bool needConfig = false;
qDebug() << Q_FUNC_INFO << servicesMap.keys();

        Q_FOREACH (const QString path, servicesList) {
            qDebug() << Q_FUNC_INFO << path << servicesMap.contains(path);

            if (servicesMap.contains(path) && servicesMap.value(path)->favorite()) {
                qDebug() << "power on, fav";
                connectToNetworkService(path);
                needConfig = false;
                return;
            } else {
                needConfig = true;
            }
        }

        if (needConfig) {
            qDebug() << Q_FUNC_INFO << "no favorite service found. Configuration needed";
            Q_EMIT wlanConfigurationNeeded();
            serviceConnect = false;
            okToConnect = false;
        }
    }
}

void QConnectionManager::connectToNetworkService(const QString &servicePath)
{
    serviceConnect = true;

    QObject::connect(servicesMap.value(servicePath), SIGNAL(connectRequestFailed(QString)),
                     this,SLOT(serviceErrorChanged(QString)), Qt::UniqueConnection);

    qDebug() << Q_FUNC_INFO << "just connect to this  thing"
             << servicesMap.value(servicePath)->type();
    qDebug() << Q_FUNC_INFO << "set idle timeout";

    NetworkTechnology *tech = netman->getTechnology(servicesMap.value(servicePath)->type());
    tech->setIdleTimeout(120);
    qDebug() << Q_FUNC_INFO << "idle timeout set";

    servicesMap.value(servicePath)->requestConnect();

//    if (servicesMap.value(servicePath)->type() == "wifi")
//        sessionAgent->setAllowedBearers(QStringList() << "wifi");// << "cellular" << "bluetooth");
//    else if (servicesMap.value(servicePath)->type() == "cellular")
//        sessionAgent->setAllowedBearers(QStringList() << "cellular");// << "wifi" << "bluetooth");
//    sessionAgent->requestConnect();

    okToConnect = false;
}


void QConnectionManager::onScanFinished()
{
    qDebug() << Q_FUNC_INFO;
}

void QConnectionManager::showNotification(const QString &/*message*/,int /*timeout*/)
{
//    qDebug() << Q_FUNC_INFO << message;
//    if (message.isEmpty())
//        return;

//    NotificationManager *manager = NotificationManager::instance();
//    QVariantHash hints;
//    hints.insert(NotificationManager::HINT_URGENCY, 2);
//    hints.insert(NotificationManager::HINT_CATEGORY, "connection.mobile");
//    uint tmpUint;
//    //    hints.insert(NotificationManager::HINT_PREVIEW_BODY, message);

//    tmpUint = manager->Notify(qApp->applicationName(), 0, QString(), QString(),
//                              QString(), QStringList(), hints, -1);
//    currentNotification = tmpUint;
}

void QConnectionManager::defaultRouteChanged(NetworkService* defaultRoute)
{
    qDebug() << Q_FUNC_INFO << defaultRoute;

    if (defaultRoute) //this apparently can be null
        Q_EMIT connectionState(defaultRoute->state(), defaultRoute->type());
    else
        Q_EMIT connectionState(QString(), QString());

}

void QConnectionManager::updateServicesMap()
{
    QStringList techPreferenceList;
    techPreferenceList << "wifi" << "cellular" << "bluetooth";
    //TODO settings
    servicesMap.clear();

    Q_FOREACH (const QString &tech,techPreferenceList) {
        QVector<NetworkService*> services = netman->getServices(tech);

        Q_FOREACH (NetworkService *serv, services) {
            servicesMap.insert(serv->path(), serv);
            qDebug() << Q_FUNC_INFO <<"::::::::::::::::::::: "<< serv->path();
            QObject::connect(serv, SIGNAL(stateChanged(QString)),
                             this,SLOT(stateChanged(QString)), Qt::UniqueConnection);
        }
    }
}

void QConnectionManager::sessionSettingsUpdated(const QVariantMap &map)
{
    qDebug() <<Q_FUNC_INFO<< map;
}

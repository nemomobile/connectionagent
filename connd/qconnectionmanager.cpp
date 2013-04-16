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

#include <connman-qt/manager.h>

#include <connman-qt/networkmanager.h>
#include <connman-qt/networktechnology.h>
#include <connman-qt/networkservice.h>
#include <connman-qt/sessionagent.h>
#include <QtDBus/QDBusConnection>

#include <Qt/qobject.h>
#include <QSettings>


QConnectionManager* QConnectionManager::self = NULL;

#define CONND_SERVICE "com.jolla.Connectiond"
#define CONND_PATH "/Connectiond"

// TODO single connection

QConnectionManager::QConnectionManager(QObject *parent) :
    QObject(parent),
     netman(NetworkManagerFactory::createInstance()),
     okToConnect(0),
     currentNetworkState(QString()),
     currentType(QString()),
     serviceConnect(0),
     currentNotification(0),
     askForRoaming(0)
{
    connectionAdaptor = new ConnAdaptor(this);
    QDBusConnection dbus = QDBusConnection::sessionBus();

    if (!dbus.registerService(CONND_SERVICE)) {
        qDebug() << "XXXXXXXXXXX could not register service XXXXXXXXXXXXXXXXXX";
    }

    if (!dbus.registerObject(CONND_PATH, this)) {
        qDebug() << "XXXXXXXXXXX could not register object XXXXXXXXXXXXXXXXXX";
    }

    askForRoaming = askRoaming();

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

    connect(netman,SIGNAL(stateChanged(QString)),this,SLOT(networkStateChanged(QString)));


    QFile connmanConf("/etc/connman/main.conf");
    if (connmanConf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!connmanConf.atEnd()) {
            QString line = connmanConf.readLine();
            if (line.startsWith("DefaultAutoConnectTechnologies")) {
                QString token = line.section(" = ",1,1).simplified();
                techPreferenceList = token.split(",");
                break;
            }
        }
        connmanConf.close();
    } else {
        techPreferenceList << "wifi" << "bluetooth" << "cellular";
    }

    updateServicesMap();
    currentNetworkState = netman->state();

    // let me control autoconnect
    netman->setSessionMode(true);


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
    Q_EMIT errorReported(error);

}

// from useragent
void QConnectionManager::onConnectionRequest()
{
    if (!autoConnect()) {
        Q_EMIT connectionRequest();
    }
}

void QConnectionManager::sendConnectReply(const QString &in0, int in1)
{
    ua->sendConnectReply(in0, in1);
}

void QConnectionManager::sendUserReply(const QVariantMap &input)
{
    ua->sendUserReply(input);
}

void QConnectionManager::onServiceAdded(const QString &servicePath)
{
    if (!servicesMap.contains(servicePath)) {
        updateServicesMap();
    }

    if (okToConnect && !currentType.isEmpty()) {
        if (servicesMap.contains(servicePath)
                && servicesMap.value(servicePath)->type() == currentType
                && servicesMap.value(servicePath)->favorite()
                && !servicesMap.value(servicePath)->roaming()) {
            connectToNetworkService(servicePath);
        } else {
            Q_EMIT configurationNeeded(servicesMap.value(servicePath)->type());
            serviceConnect = false;
        }
    }
    //automigrate
    if (servicesMap.value(servicePath)->autoConnect()) {
        connectionHandover(connectedServices.isEmpty() ? QString() : connectedServices.at(0)
                           ,findBestConnectableService());

    }
}

void QConnectionManager::onServiceRemoved(const QString &servicePath)
{
    updateServicesMap();
}

void QConnectionManager::serviceErrorChanged(const QString &error)
{
    Q_EMIT errorReported(error);
}

void QConnectionManager::serviceStateChanged(const QString &state)
{
    NetworkService *service = qobject_cast<NetworkService *>(sender());

    if (currentNetworkState == "disconnect") {
      ua->sendConnectReply("Clear");
    }
    if (state == "failure") {
        service->requestDisconnect();
        service->remove(); //reset this service
    }

    //auto migrate
    if (state == "online" || state == "ready") {
       if(!connectedServices.contains(service->path()))
           connectedServices.prepend(service->path());
    }
    //auto migrate
    if (state == "idle") {
        connectedServices.removeOne(service->path());
    }

    if (!(currentNetworkState == "online" && state == "association"))
        Q_EMIT connectionState(state, service->type());


    currentNetworkState = state;
}

bool QConnectionManager::autoConnect()
{
    QString selectedService;
    uint strength = 0;
    QString currentType;

    Q_FOREACH (const QString &servicePath, servicesMap.keys()) {

        bool isCellRoaming = false;
        if (servicesMap.value(servicePath)->type() == "cellular"
                && servicesMap.value(servicePath)->roaming()) {
            isCellRoaming = askForRoaming;
        }
        if(servicesMap.value(servicePath)->autoConnect()
                && servicesMap.value(servicePath)->favorite()
                && !isCellRoaming) {

            if (!selectedService.isEmpty()
                    && (!currentType.isEmpty()
                        && servicesMap.value(servicePath)->type() != currentType)) {
                break;
            }
            if ((servicesMap.value(servicePath)->strength() > strength)) {

                selectedService = servicePath;
                strength = servicesMap.value(servicePath)->strength();
            }
            currentType = servicesMap.value(servicePath)->type();
        }
    }

    if (!selectedService.isEmpty()) {
        serviceConnect = true;
        connectToNetworkService(selectedService);
        return true;
    }

    return false;
}

void QConnectionManager::connectToType(const QString &type)
{
    currentType = type;
    QString techPath = netman->technologyPathForType(type);

    NetworkTechnology netTech;
    netTech.setPath(techPath);

    if (!netTech.powered()) { // user has indicated they want a connection
        netTech.setPowered(true);
    }
    QStringList servicesList = netman->servicesList(type);

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

        Q_FOREACH (const QString path, servicesList) {

            if (servicesMap.contains(path) && servicesMap.value(path)->favorite()) {
                connectToNetworkService(path);
                needConfig = false;
                return;
            } else {
                needConfig = true;
            }
        }

        if (needConfig) {
            Q_EMIT configurationNeeded(type);
            serviceConnect = false;
            okToConnect = false;
        }
    }
}

void QConnectionManager::connectToNetworkService(const QString &servicePath)
{
    serviceConnect = true;
    NetworkTechnology *tech = netman->getTechnology(servicesMap.value(servicePath)->type());
    tech->setIdleTimeout(120);

    if (servicesMap.contains(servicePath))
        servicesMap.value(servicePath)->requestConnect();

    okToConnect = false;
}

void QConnectionManager::onScanFinished()
{
}

void QConnectionManager::updateServicesMap()
{
    servicesMap.clear();
    connectedServices.clear();

    Q_FOREACH (const QString &tech,techPreferenceList) {

        QVector<NetworkService*> services = netman->getServices(tech);

        Q_FOREACH (NetworkService *serv, services) {
            servicesMap.insert(serv->path(), serv);
            orderedServicesList << serv->path();
//auto migrate
            if (serv->state() == "online") {
                if(!connectedServices.contains(serv->path()))
                    connectedServices.prepend(serv->path());
            }

            QObject::connect(serv, SIGNAL(stateChanged(QString)),
                             this,SLOT(serviceStateChanged(QString)), Qt::UniqueConnection);
            QObject::connect(serv, SIGNAL(connectRequestFailed(QString)),
                             this,SLOT(serviceErrorChanged(QString)), Qt::UniqueConnection);
            QObject::connect(serv, SIGNAL(strengthChanged(uint)),
                             this,SLOT(onServiceStrengthChanged(uint)), Qt::UniqueConnection);
        }
    }

}

QString QConnectionManager::findBestConnectableService()
{

    for (int i = 0; i < orderedServicesList.count(); i++) {
        QString path = orderedServicesList.at(i);

        if (!connectedServices.contains(path)
                && servicesMap.contains(path)
                // && (servicesMap.value(path)->strength() > servicesMap.value(connectedServices.at(0))->strength())
                && (servicesMap.value(path)->state() != "online")
                && servicesMap.value(path)->autoConnect()) {
            return path;
        }
    }
    return QString();
}

void QConnectionManager::connectionHandover(const QString &oldService, const QString &newService)
{
    if (newService.isEmpty())
        return;
    if (servicesMap.contains(oldService))
        servicesMap.value(oldService)->requestDisconnect();

    if (servicesMap.contains(newService) && servicesMap.value(newService)->autoConnect())
        connectToNetworkService(newService);
}


void QConnectionManager::networkStateChanged(const QString &state)
{
    if (state == "idle") {
        //automigrate
        QString bestService =  findBestConnectableService();

        connectionHandover(connectedServices.isEmpty() ? QString() : connectedServices.at(0),
                          bestService);
    }
}

void QConnectionManager::onServiceStrengthChanged(uint level)
{
    NetworkService *service = qobject_cast<NetworkService *>(sender());
    qDebug() << Q_FUNC_INFO << service->name() << level;
}

bool QConnectionManager::askRoaming() const
{
     bool roaming;
     QSettings confFile;
     confFile.beginGroup("Connectionagent");
     roaming = confFile.value("askForRoaming").toBool();
     return roaming;
}

void QConnectionManager::setAskRoaming(bool value)
{
     QSettings confFile;
     confFile.beginGroup("Connectionagent");
     confFile.setValue("askForRoaming",value);
     askForRoaming = value;
}


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

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <connman-qt5/useragent.h>
#include <connman-qt5/networkmanager.h>
#include <connman-qt5/networktechnology.h>
#include <connman-qt5/networkservice.h>
#include <connman-qt5/sessionagent.h>
#else
#include <connman-qt/useragent.h>
#include <connman-qt/networkmanager.h>
#include <connman-qt/networktechnology.h>
#include <connman-qt/networkservice.h>
#include <connman-qt/sessionagent.h>
#endif

#include <QtDBus/QDBusConnection>

#include <QObject>
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
     currentNotification(0),
     askForRoaming(0),
     isEthernet(0),
     hasPendingReply(0),
     isConnecting(0),
     connmanPropertiesAvailable(0)
{
    qDebug() << Q_FUNC_INFO;
    connect(netman,SIGNAL(availabilityChanged(bool)),this,SLOT(connmanAvailabilityChanged(bool)));

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
    connect(ua,SIGNAL(errorReported(QString, QString)),this,SLOT(onErrorReported(QString, QString)));
    connect(ua,SIGNAL(userInputCanceled()),this,SLOT(onUserInputCanceled()));
    connect(ua,SIGNAL(userInputRequested(QString,QVariantMap)),
            this,SLOT(onUserInputRequested(QString,QVariantMap)), Qt::UniqueConnection);

    connect(netman,SIGNAL(serviceAdded(QString)),this,SLOT(onServiceAdded(QString)));
    connect(netman,SIGNAL(serviceRemoved(QString)),this,SLOT(onServiceRemoved(QString)));
    connect(netman,SIGNAL(stateChanged(QString)),this,SLOT(networkStateChanged(QString)));
    connect(netman,SIGNAL(servicesChanged()),this,SLOT(setup()));


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
    }
    if (techPreferenceList.isEmpty())
        //ethernet,bluetooth,cellular,wifi is default
        techPreferenceList << "ethernet" << "wifi" << "bluetooth" << "cellular";
}

QConnectionManager::~QConnectionManager()
{
    qDebug() << Q_FUNC_INFO <<netman->state();
    QSettings confFile;
    confFile.beginGroup("Connectionagent");
    if (netman->state() != "online")
        confFile.setValue("connected","offline");
    else
        confFile.setValue("connected","online");
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
    qDebug() << Q_FUNC_INFO;

    // gets called when a connman service gets called to connect and needs more configurations.
    Q_EMIT userInputRequested(servicePath, fields);
}

// from useragent
void QConnectionManager::onUserInputCanceled()
{
    qDebug() << Q_FUNC_INFO;

    Q_EMIT userInputCanceled();
}

// from useragent
void QConnectionManager::onErrorReported(const QString &servicePath, const QString &error)
{
    qDebug() << Q_FUNC_INFO;

    Q_EMIT errorReported(servicePath, error);
}

// from useragent
void QConnectionManager::onConnectionRequest()
{
    sendConnectReply("Suppress", 15);
    bool ok = autoConnect();
    qDebug() << Q_FUNC_INFO << ok;
    if (!ok) {
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
    qDebug() << Q_FUNC_INFO << servicePath;
    if (!servicesMap.contains(servicePath)) {
        updateServicesMap();
    }

    if (okToConnect && !currentType.isEmpty() && servicesMap.contains(servicePath)) {
        if (servicesMap.value(servicePath)->type() == currentType
                && servicesMap.value(servicePath)->favorite()
                && !servicesMap.value(servicePath)->roaming()) {
            connectToNetworkService(servicePath);
        } else {
            Q_EMIT configurationNeeded(servicesMap.value(servicePath)->type());
        }
    }
    //automigrate
    if (servicesMap.contains(servicePath)) {
        if (servicesMap.value(servicePath)->autoConnect()) {
            connectionHandover(connectedServices.isEmpty() ? QString() : connectedServices.at(0)
                                                             ,findBestConnectableService());
        }
    }
}

void QConnectionManager::onServiceRemoved(const QString &/*servicePath*/)
{
    qDebug() << Q_FUNC_INFO;
    updateServicesMap();
}

void QConnectionManager::serviceErrorChanged(const QString &error)
{
    NetworkService *service = qobject_cast<NetworkService *>(sender());
    Q_EMIT errorReported(service->path(),error);
}

void QConnectionManager::serviceStateChanged(const QString &state)
{
    NetworkService *service = qobject_cast<NetworkService *>(sender());
    qDebug() << Q_FUNC_INFO
             << service->name()
             << state
             << service->path()
             << service->ethernet()["Interface"].toString();

    if (currentNetworkState == "disconnect") {
      ua->sendConnectReply("Clear");
    }
    if (state == "failure") {
        service->requestDisconnect();
        isConnecting = false;
        okToConnect = true;
        Q_EMIT errorReported(service->path(), "Connection failure: "+ service->name());
        qDebug() << Q_FUNC_INFO << "Connection failure:"<< service->name()
                 << service->path();
    }

    //auto migrate
    if (state == "online" || state == "ready") {
        isConnecting = false;
       if(!connectedServices.contains(service->path()))
           connectedServices.prepend(service->path());
         okToConnect = true;
    }
    //auto migrate
    if (state == "idle") {
        connectedServices.removeOne(service->path());
        if (service->type() == "ethernet") { //keep this alive
            qDebug() << Q_FUNC_INFO << "requesting connection to" << service->name()
                        << service->path();
         service->requestConnect();
        } else {
            autoConnect();
        }
    }

    if (!(currentNetworkState == "online" && state == "association"))
        Q_EMIT connectionState(state, service->type());

        //todo
    // if state == idle && service exists
    // do not autoconnect, user probably wanted to disconnect
        if (currentNetworkState == "disconnect" && state == "idle"
            && servicesMap.contains(service->path())
                && service->type() != "ethernet") {
            okToConnect = false;
        }

        currentNetworkState = state;
        QSettings confFile;
        confFile.beginGroup("Connectionagent");
        confFile.setValue("connected",currentNetworkState);
}

bool QConnectionManager::autoConnect()
{
    QString selectedService;
    uint strength = 0;
    QString currentType;

    Q_FOREACH (const QString &servicePath, servicesMap.keys()) {

        if(servicesMap.value(servicePath)->state() == "configuration"
                || servicesMap.value(servicePath)->state() == "association") {
            break;
        }
        //explicitly activate ethernet service
        if(servicesMap.value(servicePath)->type() == "ethernet"
                && servicesMap.value(servicePath)->state() == "idle") {
            connectToNetworkService(servicePath);
            currentType = servicesMap.value(servicePath)->type();
            break;
        } else {

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
            connectToNetworkService(selectedService);
            return true;
        }
    } // end loop
    return false;
}

void QConnectionManager::connectToType(const QString &type)
{
    currentType = type;
    QString techPath = netman->technologyPathForType(type);
    qDebug() << Q_FUNC_INFO << techPath;

    if (techPath.isEmpty()) {
        Q_EMIT errorReported("","Type not valid");
        return;
    }

    NetworkTechnology netTech;
    netTech.setPath(techPath);

    if (!netTech.powered()) { // user has indicated they want a connection
        netTech.setPowered(true);
    }
    QStringList servicesList = netman->servicesList(type);
    bool needConfig = false;

    if (servicesList.isEmpty()) {
        if (type == "wifi") {
            okToConnect = true;
            QObject::connect(&netTech,SIGNAL(scanFinished()),this,SLOT(onScanFinished()));
            netTech.scan();
        } else {
            qDebug() << Q_FUNC_INFO << "services list is empty";
            needConfig = true;
//            Q_EMIT errorReported("Service not found"); ?? do we want to report an error
        }
    } else {
        currentType = "";

        Q_FOREACH (const QString path, servicesList) {
            // try harder with cell. a favorite is one that has been connected
            // if there is a context configured but not yet connected, try to connect anyway
            if (servicesMap.contains(path) &&
                    (servicesMap.value(path)->favorite()
                     || servicesMap.value(path)->type() == "cellular")) {
                connectToNetworkService(path);
                needConfig = false;
                return;
            } else {
                needConfig = true;
            }
        }
    }
    if (needConfig) {
        Q_EMIT configurationNeeded(type);
        okToConnect = false;
    }
}

void QConnectionManager::connectToNetworkService(const QString &servicePath)
{
    qDebug() << Q_FUNC_INFO << servicePath;

    if (servicesMap.contains(servicePath)
            && (servicesMap.value(servicePath)->state() != "online"
                || servicesMap.value(servicePath)->state() != "ready")) {
        qDebug() << Q_FUNC_INFO << "requesting connection to " << servicesMap.value(servicePath)->name();
        servicesMap.value(servicePath)->requestConnect();
        isConnecting = true;
    }
    if(!servicePath.contains("cable"))
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
        qDebug() << Q_FUNC_INFO;
    if (newService.isEmpty())
        return;
    if (servicesMap.contains(oldService)) {
        qDebug() << Q_FUNC_INFO << "requesting disconnection to" << servicesMap.value(oldService)->name();
        servicesMap.value(oldService)->requestDisconnect();
    }

    if (servicesMap.contains(newService) && servicesMap.value(newService)->autoConnect())
        connectToNetworkService(newService);
}


void QConnectionManager::networkStateChanged(const QString &state)
{
    qDebug() << Q_FUNC_INFO << state;
    if (state == "idle" && okToConnect) {
        //automigrate
        QString bestService = findBestConnectableService();

        connectionHandover(connectedServices.isEmpty() ? QString() : connectedServices.at(0),
                          bestService);
    }
}

void QConnectionManager::onServiceStrengthChanged(uint /*level*/)
{
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

void QConnectionManager::connmanAvailabilityChanged(bool b)
{
    qDebug() << Q_FUNC_INFO << b;
    connmanPropertiesAvailable = b;
    if (!b) {
        connect(netman,SIGNAL(servicesChanged()),this,SLOT(setup()));
    }
    currentNetworkState = netman->state();
}

void QConnectionManager::setup()
{
    if (!connmanPropertiesAvailable) {
        connmanPropertiesAvailable = true;
        qDebug() << Q_FUNC_INFO
                 << "Session mode"
                 << netman->sessionMode()
                 << netman->state();

        // let me control autoconnect
        if (!netman->sessionMode())
            netman->setSessionMode(true);

        if (netman->servicesList("ethernet").count() > 0)
            isEthernet = true;

        updateServicesMap();

        QSettings confFile;
        confFile.beginGroup("Connectionagent");


        if (netman->state() != "online"
                && (!isEthernet || confFile.value("connected", "online").toString() == "online")) {
            autoConnect();
        }
        disconnect(netman,SIGNAL(servicesChanged()),this,SLOT(setup()));
    }
}

void QConnectionManager::connectNotify(const QMetaMethod &/*signal*/)
{
}

void QConnectionManager::disconnectNotify(const QMetaMethod &/*signal*/)
{
}

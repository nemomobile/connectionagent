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
#define CONND_SESSION_PATH = "/ConnectionSession"

// TODO single connection

QConnectionManager::QConnectionManager(QObject *parent) :
    QObject(parent),
     netman(NetworkManagerFactory::createInstance()),
     currentNetworkState(QString()),
     currentType(QString()),
     currentNotification(0),
     askForRoaming(0),
     isEthernet(0),
     connmanAvailable(0),
     handoverInProgress(0)
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
    connect(ua,SIGNAL(browserRequested(QString,QString)),
            this,SLOT(browserRequest(QString,QString)), Qt::UniqueConnection);

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
        techPreferenceList << "bluetooth" << "wifi" << "cellular" << "ethernet" ;

    connmanAvailable = QDBusConnection::systemBus().interface()->isServiceRegistered("net.connman");
    if (connmanAvailable)
        setup();
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
void QConnectionManager::onErrorReported(const QString &servicePath, const QString &error)
{
    Q_EMIT errorReported(servicePath, error);
}

// from useragent
void QConnectionManager::onConnectionRequest()
{
    sendConnectReply("Suppress", 15);
    bool ok = autoConnect();
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
    if (!servicesMap.contains(servicePath)) {
        updateServicesMap();
    }
    //automigrate
    // is network is connected, is this a better one?
    autoConnect();
}

void QConnectionManager::onServiceRemoved(const QString &servicePath)
{
    updateServicesMap();
    if (!handoverInProgress)
        autoConnect();
}

void QConnectionManager::serviceErrorChanged(const QString &error)
{
    NetworkService *service = qobject_cast<NetworkService *>(sender());
    Q_EMIT errorReported(service->path(),error);
}

void QConnectionManager::serviceStateChanged(const QString &state)
{
    NetworkService *service = qobject_cast<NetworkService *>(sender());
    if (currentNetworkState == "disconnect") {
        ua->sendConnectReply("Clear");
    }
    if (state == "failure") {
        service->requestDisconnect();
        handoverInProgress = false;

        Q_EMIT errorReported(service->path(), "Connection failure: "+ service->name());
    }

    //auto migrate
    if (state == "online" || state == "ready") {
        handoverInProgress = false;
        lastConnectedService = service->path();

       if(!connectedServices.contains(service->path()))
           connectedServices.insert(0,service->path());
    }
    //auto migrate
    if (state == "idle") {
        connectedServices.removeOne(service->path());
        if (service->type() == "ethernet") { //keep this alive
            NetworkTechnology tech;
            tech.setPath(netman->technologyPathForService(service->path()));
            if (tech.powered()) {
                service->requestConnect();
            }
        } else {
            updateServicesMap();
            autoConnect();
        }
    }

    if (!(currentNetworkState == "online" && state == "association"))
        Q_EMIT connectionState(state, service->type());

        //todo
    // if state == idle && service exists
    // do not autoconnect, user probably wanted to disconnect
//        if (currentNetworkState == "disconnect" && state == "idle"
//            && servicesMap.contains(service->path())
//                && service->type() != "ethernet") {
//        }

        currentNetworkState = state;
        QSettings confFile;
        confFile.beginGroup("Connectionagent");
        confFile.setValue("connected",currentNetworkState);
}

bool QConnectionManager::autoConnect()
{
    QString selectedService;

    Q_FOREACH (const QString &servicePath, orderedServicesList) {
        if(servicesMap.value(servicePath)->state() == "configuration"
                || servicesMap.value(servicePath)->state() == "association") {
            break;
        }
//        //explicitly activate ethernet service
//        if(servicesMap.value(servicePath)->type() == "ethernet"
//                && servicesMap.value(servicePath)->state() == "idle") {
//            connectToNetworkService(servicePath);
//            currentType = servicesMap.value(servicePath)->type();
//            return true;
//        }
    }

    if (selectedService.isEmpty()) {
        selectedService = findBestConnectableService();
    }
    if (!selectedService.isEmpty()) {
        if (netman->state() == "online" || netman->state() == "ready") {
            connectionHandover(connectedServices.isEmpty() ? QString() : connectedServices.at(0)
                                                             ,selectedService);
        } else {
            handoverInProgress = true;
            connectToNetworkService(selectedService);
            currentType = servicesMap.value(selectedService)->type();
        }
        return true;
    }
    return false;
}

void QConnectionManager::connectToType(const QString &type)
{
    currentType = type;
    QString techPath = netman->technologyPathForType(type);

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
            QObject::connect(&netTech,SIGNAL(scanFinished()),this,SLOT(onScanFinished()));
            netTech.scan();
        } else {
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
    }
}

void QConnectionManager::connectToNetworkService(const QString &servicePath)
{
    if (!servicesMap.contains(servicePath))
        return;

    NetworkTechnology technology;
    QString type = servicesMap.value(servicePath)->type();
    if (type.isEmpty())
        return;
    technology.setPath(netman->technologyPathForType(type));

    if (technology.powered() && handoverInProgress && servicesMap.contains(servicePath)
            &&  servicesMap.value(servicePath)->state() == "idle") {

        servicesMap.value(servicePath)->requestConnect();
    }
}

void QConnectionManager::onScanFinished()
{
}

void QConnectionManager::updateServicesMap()
{
    servicesMap.clear();
    connectedServices.clear();
    orderedServicesList.clear();
    Q_FOREACH (const QString &tech,techPreferenceList) {

        QVector<NetworkService*> services = netman->getServices(tech);

        Q_FOREACH (NetworkService *serv, services) {
            servicesMap.insert(serv->path(), serv);
            orderedServicesList << serv->path();
            if (serv->state() == "online"
                    || serv->state() == "ready") {

                if(!connectedServices.contains(serv->path()))
                    connectedServices.insert(0,serv->path());

                if (netman->state() != "online") {
                    Q_EMIT connectionState(serv->state(), serv->type());
                }
            }

            QObject::connect(serv, SIGNAL(stateChanged(QString)),
                             this,SLOT(serviceStateChanged(QString)), Qt::UniqueConnection);
            QObject::connect(serv, SIGNAL(connectRequestFailed(QString)),
                             this,SLOT(serviceErrorChanged(QString)), Qt::UniqueConnection);

            QObject::connect(serv, SIGNAL(errorChanged(QString)),
                             this,SLOT(servicesError(QString)), Qt::UniqueConnection);
            QObject::connect(serv, SIGNAL(strengthChanged(uint)),
                             this,SLOT(onServiceStrengthChanged(uint)), Qt::UniqueConnection);

        }
    }
}

void QConnectionManager::servicesError(const QString &errorMessage)
{
    qDebug() << Q_FUNC_INFO << errorMessage;
}

QString QConnectionManager::findBestConnectableService()
{
    for (int i = 0; i < orderedServicesList.count(); i++) {

        QString path = orderedServicesList.at(i);

        NetworkService *service = servicesMap.value(path);

        if (service->state() != "idle")
            return QString();

        if (lastConnectedService == service->path())
            break;

        NetworkTechnology technology;
        technology.setPath(netman->technologyPathForType(servicesMap.value(path)->type()));

        bool isCellRoaming = false;
        if (service->type() == "cellular"
                && service->roaming()) {
            isCellRoaming = askForRoaming;
        }

        if (!connectedServices.contains(path)
                && servicesMap.contains(path)
                // && (servicesMap.value(path)->strength() > servicesMap.value(connectedServices.at(0))->strength())
                && service->autoConnect()
                && service->favorite()
                && !isCellRoaming) {
            return path;
        }
    }
    return QString();
}

void QConnectionManager::connectionHandover(const QString &oldService, const QString &newService)
{
    bool isOnline = false;
    bool ok = true;
    if (newService.isEmpty())
        return;

    if (servicesMap.contains(oldService))
        if (servicesMap.value(oldService)->state() == "online"
                || servicesMap.value(oldService)->state() == "ready") {

            isOnline = true;

            if (servicesMap.contains(newService)
                    && (techPreferenceList.indexOf(servicesMap.value(newService)->type()) >
                        techPreferenceList.indexOf(servicesMap.value(oldService)->type()))) {
                ok = false;
            }
        }
    if (connectedServices.at(0) == oldService)
        isOnline = true;

    if (ok) {
        if (isOnline) {
            servicesMap.value(oldService)->requestDisconnect();
            handoverInProgress = true;
        }
    }
}


void QConnectionManager::networkStateChanged(const QString &state)
{
    if (state == "idle" && handoverInProgress) {
        //automigrate
        autoConnect();
    }
    if (state == "online")
        handoverInProgress = false;

    QSettings confFile;
    confFile.beginGroup("Connectionagent");
    if (state != "online")
        confFile.setValue("connected","offline");
    else
        confFile.setValue("connected","online");

}

void QConnectionManager::onServiceStrengthChanged(uint /*level*/)
{
   // NetworkService *service = qobject_cast<NetworkService *>(sender());
//    if (service->state() == "online")
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
    connmanAvailable = b;
    if (b) {
        setup();
        connect(netman,SIGNAL(servicesChanged()),this,SLOT(setup()));
        currentNetworkState = netman->state();
    } else {
        servicesMap.clear();
        currentNetworkState = "error";
    }
}

void QConnectionManager::emitConnectionState()
{
    Q_EMIT connectionState("idle", "All");
    Q_EMIT connectionState("online", "All");
}

void QConnectionManager::setup()
{
    if (connmanAvailable) {
        if (netman->defaultRoute()->type() == "ethernet")
            isEthernet = true;

        updateServicesMap();
        if (netman->state() == "online"
                || netman->state() == "ready") {
            lastConnectedService = netman->defaultRoute()->path();
            connectedServices.append(lastConnectedService);
        }

        QSettings confFile;
        confFile.beginGroup("Connectionagent");

        if (netman->state() != "online"
                && (!isEthernet && confFile.value("connected", "online").toString() == "online")) {
            autoConnect();
        }
        disconnect(netman,SIGNAL(servicesChanged()),this,SLOT(setup()));

        Q_FOREACH(const NetworkTechnology *technology,netman->getTechnologies()) {
            connect(technology,SIGNAL(poweredChanged(bool)),this,SLOT(technologyPowerChanged(bool)));
        }
    }
}

void QConnectionManager::technologyPowerChanged(bool b)
{
    NetworkTechnology *tech = qobject_cast<NetworkTechnology *>(sender());
    if (b && tech->type() == "wifi" || tech->type() == "cellular")
            tech->scan();
}

void QConnectionManager::browserRequest(const QString &servicePath, const QString &url)
{
    Q_EMIT requestBrowser(url);
}

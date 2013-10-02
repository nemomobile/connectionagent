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
#include <qofono-qt5/qofonoconnectioncontext.h>
#include <qofono-qt5/qofonoconnectionmanager.h>
#include <qofono-qt5/qofonomanager.h>

#else
#include <connman-qt/useragent.h>
#include <connman-qt/networkmanager.h>
#include <connman-qt/networktechnology.h>
#include <connman-qt/networkservice.h>
#include <connman-qt/sessionagent.h>
#include <qofono-qt/qofonoconnectioncontext.h>
#include <qofono-qt/qofonoconnectionmanager.h>
#include <qofono-qt/qofonomanager.h>
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

    connect(netman,SIGNAL(servicesChanged()),this,SLOT(onServicesChanged()));
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
        techPreferenceList << "wifi" << "cellular" << "bluetooth" << "ethernet";

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
    qDebug() << Q_FUNC_INFO;
    if (!self) {
        self = new QConnectionManager;
    }

    return *self;
}

// from useragent
void QConnectionManager::onUserInputRequested(const QString &servicePath, const QVariantMap &fields)
{
    qDebug() << Q_FUNC_INFO << servicePath;
    // gets called when a connman service gets called to connect and needs more configurations.
    Q_EMIT userInputRequested(servicePath, fields);
}

// from useragent
void QConnectionManager::onUserInputCanceled()
{
    qDebug() << Q_FUNC_INFO ;
    Q_EMIT userInputCanceled();
}

// from useragent
void QConnectionManager::onErrorReported(const QString &servicePath, const QString &error)
{
    qDebug() << Q_FUNC_INFO << error;
    Q_EMIT errorReported(servicePath, error);
    handoverInProgress = false;
}

// from useragent
void QConnectionManager::onConnectionRequest()
{
    qDebug() << Q_FUNC_INFO << "from usergent" << handoverInProgress;
    sendConnectReply("Suppress", 15);
    bool ok = autoConnect();
    qDebug() << Q_FUNC_INFO ;
    if (!ok) {
        Q_EMIT connectionRequest();
    }
}

void QConnectionManager::sendConnectReply(const QString &in0, int in1)
{
    qDebug() << Q_FUNC_INFO;
    ua->sendConnectReply(in0, in1);
}

void QConnectionManager::sendUserReply(const QVariantMap &input)
{
    qDebug() << Q_FUNC_INFO;
    ua->sendUserReply(input);
}

void QConnectionManager::onServicesChanged()
{
    updateServicesMap();
    if (!handoverInProgress)
        autoConnect();
}

void QConnectionManager::serviceErrorChanged(const QString &error)
{
    qDebug() << Q_FUNC_INFO << error;
    NetworkService *service = qobject_cast<NetworkService *>(sender());
    Q_EMIT errorReported(service->path(),error);
}

void QConnectionManager::serviceStateChanged(const QString &state)
{
    NetworkService *service = qobject_cast<NetworkService *>(sender());
    qDebug() << Q_FUNC_INFO << state << service->name();

    if (currentNetworkState == "disconnect") {
        ua->sendConnectReply("Clear");
    }
    if (state == "failure") {
//        service->requestDisconnect();
        handoverInProgress = false;

        if (!manuallyConnectedService.isEmpty()) {
            manuallyConnectedService.clear();
        }

        Q_EMIT errorReported(service->path(), "Connection failure: "+ service->name());
        autoConnect();
    }

//    qDebug() << Q_FUNC_INFO << serviceInProgress << handoverInProgress;
    //auto migrate
    if (service->path() == serviceInProgress
            && state == "online") {
        serviceInProgress.clear();
        handoverInProgress = false;
    }

    if (isStateOnline(state)) {
        lastConnectedService = service->path();

        if(!connectedServices.contains(service->path()))
            connectedServices.insert(0,service->path());
    }
    //auto migrate
    if (state == "idle") {
        handoverInProgress = false;

        connectedServices.removeOne(service->path());
        if (!manuallyConnectedService.isEmpty()) {
            manuallyConnectedService.clear();
        }
        if (service->type() == "ethernet") { //keep this alive
            NetworkTechnology tech;
            tech.setPath(netman->technologyPathForService(service->path()));
            if (tech.powered()) {
               requestConnect(service->path());
            }
        } else {
            updateServicesMap();
            if (!serviceInProgress.isEmpty())
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
//    if (!manuallyConnectedService.isEmpty()/*!manualConnected*/)
//        return false;

    QString selectedService;
    qDebug() << Q_FUNC_INFO
                 << "handoverInProgress"
                 << handoverInProgress
                 << orderedServicesList.count()
                 << netman->state();
//    Q_FOREACH (const QString &servicePath, orderedServicesList) {
//        if(servicesMap.contains(servicePath)
//                && servicesMap.value(servicePath)->state() == "configuration"
//                || servicesMap.value(servicePath)->state() == "association") {
//            break;
//        }
//        //explicitly activate ethernet service
//        if(servicesMap.value(servicePath)->type() == "ethernet"
//                && servicesMap.value(servicePath)->state() == "idle") {
//            connectToNetworkService(servicePath);
//            currentType = servicesMap.value(servicePath)->type();
//            return true;
//        }
//    }

    if (selectedService.isEmpty()) {
        selectedService = findBestConnectableService();
    }
    if (!selectedService.isEmpty()) {
        if (isStateOnline(netman->state())) {
            connectionHandover(connectedServices.isEmpty() ? QString() : connectedServices.at(0)
                                                             ,selectedService);
        } else {
            connectToNetworkService(selectedService);
            currentType = servicesMap.value(selectedService)->type();
        }
        return true;
    }
    return false;
}

void QConnectionManager::connectToType(const QString &type)
{
    qDebug() << Q_FUNC_INFO;
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
    qDebug() << Q_FUNC_INFO << servicePath
             << handoverInProgress
             << netman->state();

    if (!servicesMap.contains(servicePath) || !serviceInProgress.isEmpty())
        return;

    NetworkTechnology technology;
    QString type = servicesMap.value(servicePath)->type();
    if (type.isEmpty())
        return;
    technology.setPath(netman->technologyPathForType(type));
    if (servicesMap.value(servicePath)->state() != "online")
        requestDisconnect(servicePath);

    if (manuallyConnectedService.isEmpty() && technology.powered() && !handoverInProgress) {
        if (servicePath.contains("cellular")) {
// ofono active seems to work better in our case
            QOfonoManager oManager;
            QOfonoConnectionManager oConnManager;
            oConnManager.setModemPath(oManager.modems().at(0));
            Q_FOREACH (const QString &contextPath, oConnManager.contexts()) {

                if (contextPath.contains(servicePath.section("_",2,2))) {
                    qDebug() << Q_FUNC_INFO << "requesting cell connection";
                    serviceInProgress = servicePath;

                    handoverInProgress = true;
                    QOfonoConnectionContext oContext;
                    oContext.setContextPath(contextPath);
                    oContext.setActive(true);
                    return;
                }
            }

        } else {
            requestConnect(servicePath);
        }
    }
}

void QConnectionManager::onScanFinished()
{
}

void QConnectionManager::updateServicesMap()
{
    qDebug() << Q_FUNC_INFO;
    int numbServices = servicesMap.count();
    qDebug() << Q_FUNC_INFO << numbServices << netman->getServices().count();
    if (numbServices == netman->getServices().count())
        return;

    Q_FOREACH (const QString &tech,techPreferenceList) {
        QVector<NetworkService*> services = netman->getServices(tech);
        Q_FOREACH (NetworkService *serv, services) {
            if (serv->strength() == 0)
                continue;

            servicesMap.insert(serv->path(), serv);
            orderedServicesList << serv->path();
 
           // if (serv->autoConnect())
                orderedServicesList << serv->path();

            if (isStateOnline(serv->state())) {

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
            QObject::connect(serv, SIGNAL(serviceConnectionStarted()),
                             this,SLOT(onServiceConnectionStarted()));
            QObject::connect(serv, SIGNAL(serviceDisconnectionStarted()),
                             this,SLOT(onServiceDisconnectionStarted()), Qt::UniqueConnection);

        }
    }
}

void QConnectionManager::servicesError(const QString &errorMessage)
{
    NetworkService *serv = qobject_cast<NetworkService *>(sender());
    qDebug() << Q_FUNC_INFO << serv->name() << errorMessage;
    Q_EMIT onErrorReported(serv->path(), errorMessage);
}

QString QConnectionManager::findBestConnectableService()
{    
    qDebug() << Q_FUNC_INFO;

    for (int i = 0; i < orderedServicesList.count(); i++) {

        QString path = orderedServicesList.at(i);

        NetworkService *service = servicesMap.value(path);

        if (lastConnectedService == service->path()) {
            continue;
        }

        bool isCellRoaming = false;
        if (service->type() == "cellular"
                && service->roaming()) {
            isCellRoaming = askForRoaming;
        }

        if (isBestService(service->path())
                && service->favorite()
                && !isCellRoaming) {
            qDebug() << Q_FUNC_INFO << path;
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
        if (isStateOnline(servicesMap.value(oldService)->state())) {

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
            requestDisconnect(oldService);
        }
    }
}


void QConnectionManager::networkStateChanged(const QString &state)
{
    qDebug() << Q_FUNC_INFO << state;
    if (state == "online") {
        handoverInProgress = false;
    }

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
    qDebug() << Q_FUNC_INFO;
    bool roaming;
    QSettings confFile;
    confFile.beginGroup("Connectionagent");
    roaming = confFile.value("askForRoaming").toBool();
    return roaming;
}

void QConnectionManager::setAskRoaming(bool value)
{
    qDebug() << Q_FUNC_INFO;
    QSettings confFile;
    confFile.beginGroup("Connectionagent");
    confFile.setValue("askForRoaming",value);
    askForRoaming = value;
}

void QConnectionManager::connmanAvailabilityChanged(bool b)
{
    qDebug() << Q_FUNC_INFO;
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
    qDebug() << Q_FUNC_INFO
             << netman->state();
    if (connmanAvailable) {

        updateServicesMap();
        if (isStateOnline(netman->state())) {
            lastConnectedService = netman->defaultRoute()->path();
            connectedServices.append(lastConnectedService);
            handoverInProgress = false;

        if (netman->defaultRoute()->type() == "ethernet")
            isEthernet = true;
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
    if (b && (tech->type() == "wifi" || tech->type() == "cellular"))
            tech->scan();
}

void QConnectionManager::browserRequest(const QString &servicePath, const QString &url)
{
    Q_EMIT requestBrowser(url);
}

void QConnectionManager::onServiceConnectionStarted()
{
    qDebug() << Q_FUNC_INFO;
    NetworkService *serv = qobject_cast<NetworkService *>(sender());
    manuallyConnectedService = serv->path();
    handoverInProgress = true;
    serviceInProgress = serv->path();
}

bool QConnectionManager::isBestService(const QString &servicePath)
{
    qDebug() << Q_FUNC_INFO;
    if (servicePath == autoDisconnectService) return false;
    if (servicesMap.contains(servicePath) && servicesMap.value(servicePath)->strength() == 0) return false;
//    if (connectedServices.contains(servicePath)) return false;
    if (netman->defaultRoute()->path().contains(servicePath)) return false;
    if (netman->defaultRoute()->state() != "online") return true;
    int dfIndex = orderedServicesList.indexOf(netman->defaultRoute()->path());
    if (dfIndex == -1) return true;
    if (orderedServicesList.indexOf(servicePath) < dfIndex) return true;
    return false;
}

bool QConnectionManager::isStateOnline(const QString &state)
{
    if (state == "online" || state == "ready")
        return true;
    return false;
}
void QConnectionManager::requestDisconnect(const QString &service)
{
    if (servicesMap.contains(service)) {
        qDebug() << Q_FUNC_INFO << service;
        servicesMap.value(service)->requestDisconnect();
        autoDisconnectService = service;
    }
}

void QConnectionManager::requestConnect(const QString &service)
{
    if (servicesMap.contains(service)) {
        qDebug() << Q_FUNC_INFO << service;
        handoverInProgress = true;
        servicesMap.value(service)->requestConnect();
    }
}


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
#include "wakeupwatcher.h"

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <connman-qt5/useragent.h>
#include <connman-qt5/networkmanager.h>
#include <connman-qt5/networktechnology.h>
#include <connman-qt5/networkservice.h>
#include <connman-qt5/sessionagent.h>
#include <qofono-qt5/qofonoconnectioncontext.h>
#include <qofono-qt5/qofonoconnectionmanager.h>
#include <qofono-qt5/qofononetworkregistration.h>
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

QConnectionManager::QConnectionManager(QObject *parent) :
    QObject(parent),
     netman(NetworkManagerFactory::createInstance()),
     currentNetworkState(QString()),
     currentType(QString()),
     currentNotification(0),
     askForRoaming(false),
     isEthernet(false),
     connmanAvailable(false),
     handoverInProgress(false),
     oContext(0),
     tetheringWifiTech(0),
     tetheringEnabled(false),
     flightModeSuppression(false)
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

    connect(netman,SIGNAL(servicesListChanged(QStringList)),this,SLOT(servicesListChanged(QStringList)));
    connect(netman,SIGNAL(stateChanged(QString)),this,SLOT(networkStateChanged(QString)));
    connect(netman,SIGNAL(servicesChanged()),this,SLOT(setup()));
    connect(netman,SIGNAL(offlineModeChanged(bool)),this,SLOT(offlineModeChanged(bool)));

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

    mceWatch = new WakeupWatcher(this);
    connect(mceWatch,SIGNAL(displayStateChanged(QString)),this,SLOT(displayStateChanged(QString)));
    connect(mceWatch,SIGNAL(sleepStateChanged(bool)),this,SLOT(sleepStateChanged(bool)));

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
    qDebug() << servicePath;
    // gets called when a connman service gets called to connect and needs more configurations.
    Q_EMIT userInputRequested(servicePath, fields);
}

// from useragent
void QConnectionManager::onUserInputCanceled()
{
    qDebug() ;
    Q_EMIT userInputCanceled();
}

// from useragent
void QConnectionManager::onErrorReported(const QString &servicePath, const QString &error)
{
    qDebug() << error;
    Q_EMIT errorReported(servicePath, error);
    serviceInProgress.clear();
}

// from useragent
void QConnectionManager::onConnectionRequest()
{
    sendConnectReply("Suppress", 15);
    bool ok = autoConnect();
    qDebug() << serviceInProgress << ok << flightModeSuppression;
    if (!ok && serviceInProgress.isEmpty() && !flightModeSuppression) {
        Q_EMIT connectionRequest();
    }
}

void QConnectionManager::sendConnectReply(const QString &in0, int in1)
{
    ua->sendConnectReply(in0, in1);
}

void QConnectionManager::sendUserReply(const QVariantMap &input)
{
    qDebug() << Q_FUNC_INFO;
    ua->sendUserReply(input);
}

void QConnectionManager::servicesListChanged(const QStringList &list)
{
    bool ok = false;
    Q_FOREACH(const QString &path,list) {
        if (orderedServicesList.indexOf((path)) == -1) {
         //added
            qDebug() << Q_FUNC_INFO << "added" << path;
            serviceAdded(path);
            ok = true;
        }
    }
    Q_FOREACH(const QString &path,orderedServicesList) {
        if (list.indexOf((path)) == -1) {
            qDebug() << Q_FUNC_INFO << "removed" << path;
            serviceRemoved(path);
            ok = true;
         //removed
        }
    }
    if (ok && serviceInProgress.isEmpty())
        autoConnect();
}

void QConnectionManager::serviceErrorChanged(const QString &error)
{
    qDebug() << error;
    NetworkService *service = static_cast<NetworkService *>(sender());
    Q_EMIT errorReported(service->path(),error);
}

void QConnectionManager::serviceStateChanged(const QString &state)
{
    NetworkService *service = static_cast<NetworkService *>(sender());
    qDebug() << state << service->name();

    if (currentNetworkState == "disconnect") {
        ua->sendConnectReply("Clear");
    }
    if (state == "failure") {
        serviceInProgress.clear();

        Q_EMIT errorReported(service->path(), "Connection failure: "+ service->name());
        autoConnect();
    }

    //auto migrate
    if (service->path() == serviceInProgress
            && state == "online") {
        serviceInProgress.clear();
    }

    //auto migrate
    if (state == "idle") {

        if (serviceInProgress == service->path())
            serviceInProgress.clear();

        previousConnectedService = service->path();
        if (service->type() == "ethernet") { //keep this alive
            NetworkTechnology tech;
            tech.setPath(netman->technologyPathForService(service->path()));
            if (tech.powered()) {
               requestConnect(service->path());
            }
        } else {
            updateServicesMap();
            qDebug() <<"serviceInProgress"<< serviceInProgress;
            if (!serviceInProgress.isEmpty())
                autoConnect();
        }
    }

    if (!(currentNetworkState == "online" && state == "association"))
        Q_EMIT connectionState(state, service->type());

        currentNetworkState = state;
        QSettings confFile;
        confFile.beginGroup("Connectionagent");
        confFile.setValue("connected",currentNetworkState);
}

bool QConnectionManager::autoConnect()
{
    QString selectedService;
    qDebug() << "serviceInProgress"
             << serviceInProgress
             << orderedServicesList.count()
             << netman->defaultRoute()->name()
             << netman->state()
             << tetheringEnabled;

    if (tetheringEnabled || !serviceInProgress.isEmpty())
        return false;

    if (selectedService.isEmpty()) {
        selectedService = findBestConnectableService();
    }
    if (!selectedService.isEmpty()) {
        bool ok = connectToNetworkService(selectedService);
        if (ok)
            currentType = servicesMap.value(selectedService)->type();
        return  ok;
    }
    return false;
}

void QConnectionManager::connectToType(const QString &type)
{
    qDebug() << type;
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

bool QConnectionManager::connectToNetworkService(const QString &servicePath)
{
    qDebug() << servicePath
             << netman->state();

    if (!servicesMap.contains(servicePath) || !serviceInProgress.isEmpty())
        return false;

    NetworkTechnology technology;
    QString type = servicesMap.value(servicePath)->type();
    if (type.isEmpty())
        return false;
    technology.setPath(netman->technologyPathForType(type));

    if (servicesMap.value(servicePath)->state() != "online")
        requestDisconnect(netman->defaultRoute()->path());

    if (technology.powered()) {
        if (servicePath.contains("cellular")) {

            QOfonoManager oManager;
            if (!oManager.available()) {
                qDebug() << "ofono not available.";
                return false;
            }

            QOfonoConnectionManager oConnManager;
            oConnManager.setModemPath(oManager.modems().at(0));

            QOfonoNetworkRegistration ofonoReg;
            ofonoReg.setModemPath(oManager.modems().at(0));
            if (ofonoReg.status() != "registered"
                    &&  ofonoReg.status() != "roaming") { //not on any cell network so bail
                qDebug() << "ofono is not registered yet";
                return false;
            }
            //isCellRoaming
            bool ok = true;
            if (servicesMap.value(servicePath)->roaming()) {
                if (oConnManager.roamingAllowed()) {
                    if (askRoaming()) {
                        // ask user
                    }
                } else {
                    //roaming and user doesnt want connection while roaming
                    qDebug() << "roaming not allowed";
                    return false;
                }
            }

            if (ok)
                requestConnect(servicePath);
        } else {
            requestConnect(servicePath);
        }
    }
    return true;
}

void QConnectionManager::onScanFinished()
{
}

void QConnectionManager::updateServicesMap()
{
    qDebug() << Q_FUNC_INFO;
    QStringList oldServices = orderedServicesList;
    orderedServicesList.clear();

    Q_FOREACH (const QString &tech,techPreferenceList) {
        QVector<NetworkService*> services = netman->getServices(tech);

        Q_FOREACH (NetworkService *serv, services) {

            servicesMap.insert(serv->path(), serv);
            orderedServicesList << serv->path();

            if (!oldServices.contains(serv->path())) {
                //new!
                qDebug() <<"new service"
                         << serv->path();

                QObject::connect(serv, SIGNAL(stateChanged(QString)),
                                 this,SLOT(serviceStateChanged(QString)), Qt::UniqueConnection);
                QObject::connect(serv, SIGNAL(connectRequestFailed(QString)),
                                 this,SLOT(serviceErrorChanged(QString)), Qt::UniqueConnection);

                QObject::connect(serv, SIGNAL(errorChanged(QString)),
                                 this,SLOT(servicesError(QString)), Qt::UniqueConnection);
                QObject::connect(serv, SIGNAL(strengthChanged(uint)),
                                 this,SLOT(onServiceStrengthChanged(uint)), Qt::UniqueConnection);
                QObject::connect(serv, SIGNAL(serviceConnectionStarted()),
                                 this,SLOT(onServiceConnectionStarted()), Qt::UniqueConnection);
                QObject::connect(serv, SIGNAL(serviceDisconnectionStarted()),
                                 this,SLOT(onServiceDisconnectionStarted()), Qt::UniqueConnection);
            }
        }
    }
    qDebug() << orderedServicesList;
}

void QConnectionManager::servicesError(const QString &errorMessage)
{
    NetworkService *serv = static_cast<NetworkService *>(sender());
    qDebug() << serv->name() << errorMessage;
    Q_EMIT onErrorReported(serv->path(), errorMessage);
    serviceInProgress.clear();
}

void QConnectionManager::ofonoServicesError(const QString &errorMessage)
{
    QOfonoConnectionContext *context = static_cast<QOfonoConnectionContext *>(sender());
    QVector<NetworkService*> services = netman->getServices("cellular");
    Q_FOREACH (NetworkService *serv, services) {
        if (context->contextPath().contains(serv->path().section("_",2,2))) {
            Q_EMIT onErrorReported(serv->path(), errorMessage);
            serviceInProgress.clear();
            qDebug() << serv->name() << errorMessage;
            return;
        }
    }
    qWarning() << "ofono error but could not discover connman service";
}

QString QConnectionManager::findBestConnectableService()
{
    for (int i = 0; i < orderedServicesList.count(); i++) {

        QString path = orderedServicesList.at(i);

        NetworkService *service = servicesMap.value(path);
        qDebug() << "looking at" << service->name() << service->autoConnect();

        bool online = isStateOnline(netman->defaultRoute()->state());
        if (online && netman->defaultRoute()->path() == service->path())
            return QString();//best already connected

        if (!service->autoConnect()) {
            continue;
        }
        qDebug() << previousConnectedService << service->path();

        if (!online && previousConnectedService == service->path()) {
            continue;
        }

        qDebug() <<Q_FUNC_INFO<< "continued"
                << online
                << (netman->defaultRoute()->path() == service->path());

        bool isCellRoaming = false;
        if (service->type() == "cellular"
                && service->roaming()) {
                    isCellRoaming = askForRoaming;
        }

        if (isBestService(service->path())
                && service->favorite()
                && !isCellRoaming) {
            qDebug() << path;
            return path;
        }
    }
    return QString();
}

void QConnectionManager::networkStateChanged(const QString &state)
{
    qDebug() << state;

    QSettings confFile;
    confFile.beginGroup("Connectionagent");
    if (state != "online")
        confFile.setValue("connected","offline");
    else
        confFile.setValue("connected","online");
}

void QConnectionManager::onServiceStrengthChanged(uint /*level*/)
{
// NetworkService *service = static_cast<NetworkService *>(sender());
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

void QConnectionManager::serviceAdded(const QString &srv)
{
    qDebug() << Q_FUNC_INFO << "<<<<"<< srv;
    updateServicesMap();
}

void QConnectionManager::serviceRemoved(const QString &srv)
{
    qDebug() << Q_FUNC_INFO << "<<<<" << srv;
    if (orderedServicesList.contains(srv)) {
        orderedServicesList.removeOne(srv);
    }
    if (serviceInProgress == srv)
        serviceInProgress.clear();
}

void QConnectionManager::setup()
{
    qDebug() << Q_FUNC_INFO
             << connmanAvailable;

    if (connmanAvailable) {
        qDebug() << Q_FUNC_INFO
                 << netman->state()
                 << netman->defaultRoute()->type();
        techChanged();
        connect(netman,SIGNAL(technologiesChanged()),this,SLOT(techChanged()));
        updateServicesMap();
        offlineModeChanged(netman->offlineMode());

        if (isStateOnline(netman->state())) {
            previousConnectedService = netman->defaultRoute()->path();

            if (netman->defaultRoute()->type() == "ethernet")
                isEthernet = true;
        } else
            netman->setSessionMode(true); //turn off connman autoconnecting
                                          // in later versions (1.18) this won't effect autoconnect
                                          // so we will have to turn it off some other way

        QSettings confFile;
        confFile.beginGroup("Connectionagent");
        qDebug() << "config file says" << confFile.value("connected", "online").toString();
        if (netman->state() != "online"
                && (!isEthernet && confFile.value("connected", "online").toString() == "online")) {
            autoConnect();
        }
        disconnect(netman,SIGNAL(servicesChanged()),this,SLOT(setup()));
    }
}

void QConnectionManager::technologyPowerChanged(bool b)
{
    NetworkTechnology *tech = static_cast<NetworkTechnology *>(sender());
    if (b && (tech->type() == "wifi" || tech->type() == "cellular"))
        tech->scan();
}

void QConnectionManager::techChanged()
{
    Q_FOREACH(const NetworkTechnology *technology,netman->getTechnologies()) {
        connect(technology,SIGNAL(poweredChanged(bool)),this,SLOT(technologyPowerChanged(bool)));
    }
    tetheringWifiTech = netman->getTechnology("wifi");

    if (tetheringWifiTech) {
        tetheringEnabled = tetheringWifiTech->tethering();
        qDebug() << "tethering is" << tetheringEnabled;
        QObject::connect(tetheringWifiTech, SIGNAL(tetheringChanged(bool)),
                         this,SLOT(techTetheringChanged(bool)), Qt::UniqueConnection);
    }
}

void QConnectionManager::browserRequest(const QString &servicePath, const QString &url)
{
    Q_EMIT requestBrowser(url);
}

void QConnectionManager::onServiceConnectionStarted()
{
    qDebug() << Q_FUNC_INFO;
    NetworkService *serv = static_cast<NetworkService *>(sender());
    serviceInProgress = serv->path();
}

void QConnectionManager::onServiceDisconnectionStarted()
{
//    NetworkService *serv = static_cast<NetworkService *>(sender());
    qDebug() << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<" ;
}

bool QConnectionManager::isBestService(const QString &servicePath)
{
    qDebug() << Q_FUNC_INFO
             << servicePath;
    if (tetheringEnabled) return false;
    if (netman->defaultRoute()->path().contains(servicePath)) return false;
    if (!serviceInProgress.isEmpty() && serviceInProgress != servicePath) return false;
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

void QConnectionManager::requestDisconnect(const QString &servicePath)
{
    if (servicesMap.contains(servicePath)) {
        qDebug() << servicePath << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<";

        QDBusInterface service("net.connman", servicePath.toLocal8Bit(),
                               "net.connman.Service", QDBusConnection::systemBus());
        QDBusMessage reply = service.call(QDBus::NoBlock, QStringLiteral("Disconnect"));
    }
}

void QConnectionManager::requestConnect(const QString &servicePath)
{
    if (servicesMap.contains(servicePath)) {
        qDebug() << servicePath;
         serviceInProgress = servicePath;

        QDBusInterface service("net.connman", servicePath.toLocal8Bit(),
                               "net.connman.Service", QDBusConnection::systemBus());
        QDBusMessage reply = service.call(QDBus::NoBlock, QStringLiteral("Connect"));
        manualConnected = false;
        autoConnectService = servicePath;
    }
}

void QConnectionManager::techTetheringChanged(bool b)
{
    qDebug() << b;
    tetheringEnabled = b;
}

void QConnectionManager::offlineModeChanged(bool b)
{
    flightModeSuppression = b;
    if (b) {
        previousConnectedService.clear();
        QTimer::singleShot(5 * 1000 * 60,this,SLOT(flightModeDialogSuppressionTimeout())); //5 minutes
    }
}

void QConnectionManager::flightModeDialogSuppressionTimeout()
{
    if (flightModeSuppression)
        flightModeSuppression = false;
}

void QConnectionManager::displayStateChanged(const QString &state)
{
    if (state == "on") {
        NetworkTechnology *wifiTech = netman->getTechnology("wifi");
        if (wifiTech->powered() && !wifiTech->connected()) {
            previousConnectedService.clear();
            wifiTech->scan();
        }
    }
}

void QConnectionManager::sleepStateChanged(bool on)
{
}

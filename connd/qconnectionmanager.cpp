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
     flightModeSuppression(false),
     scanTimeoutInterval(1)
{
    qDebug() << Q_FUNC_INFO;

    manualConnnectionTimer.invalidate();
    manualDisconnectionTimer.invalidate();

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
    goodConnectTimer = new QTimer(this);
    goodConnectTimer->setSingleShot(true);
    connect(goodConnectTimer,SIGNAL(timeout()),this,SLOT(goodConnectionTimeout()));

    scanTimer = new QTimer(this);
    connect(scanTimer,SIGNAL(timeout()),this,SLOT(scanTimeout()));
    scanTimer->setSingleShot(true);
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
    manualDisconnectionTimer.invalidate();
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
            ok = true;
        }
    }
    if (ok)
        updateServicesMap();

    Q_FOREACH(const QString &path,orderedServicesList) {
        if (list.indexOf((path)) == -1) {
            qDebug() << Q_FUNC_INFO << "removed" << path;
            serviceRemoved(path);
            ok = true;
         //removed
        }
    }
    if (ok && serviceInProgress.isEmpty())
        QTimer::singleShot(1000,this,SLOT(delayedAutoconnect()));
}

void QConnectionManager::delayedAutoconnect()
{
    autoConnect();
}

void QConnectionManager::serviceErrorChanged(const QString &error)
{
    qDebug() << error;
    if (error == "Operation aborted")
        return;
    NetworkService *service = static_cast<NetworkService *>(sender());
    if (error == "connect-failed")
        service->requestDisconnect();
    if (error != "In progress")
        Q_EMIT errorReported(service->path(),error);
}

void QConnectionManager::serviceStateChanged(const QString &state)
{
    NetworkService *service = static_cast<NetworkService *>(sender());
    qDebug() << state << service->name() << service->strength();
    if (!service->favorite() || !netman->getTechnology(service->type())->powered()) {
        qDebug() << "not fav or not powered";
        return;
    }
    if (state == "disconnect") {
        ua->sendConnectReply("Clear");
    }
    if (state == "failure") {
        serviceInProgress.clear();
        service->requestDisconnect();
    }
    if (currentNetworkState == "idle" && state == "association") {
        qDebug() << "Starting good connection timer";
        goodConnectTimer->setInterval(120 * 1000); //2 minutes, same as connman input request timeout
        goodConnectTimer->start();
    }

    //manual connection
    if (currentNetworkState == "ready" && state == "online" && service->path() != serviceInProgress) {
        qDebug() << "manual connection of" << service->path() << "detected, enabling auto connect timeout";
        lastManuallyConnectedService = service->path();
        manualConnnectionTimer.start();
    }

    if (service->path() == serviceInProgress
            && state == "online") {
        Q_EMIT connectionState(state, service->type());
        serviceInProgress.clear();
    }

    //auto migrate
    if (state == "idle") {
        if (lastManuallyConnectedService == service->path()) {
            lastManuallyConnectedService.clear();
            manualConnnectionTimer.invalidate();
        }

        if (!delayedConnectService.isEmpty()) {
            delayedConnect();
        }

        if (serviceInProgress == service->path())
            serviceInProgress.clear();
        if (service->strength() > 0) {
            lastManuallyDisconnectedService = service->path();
            manualDisconnectionTimer.start();
        }
        if (service->type() == "ethernet") { //keep this alive
            NetworkTechnology tech;
            tech.setPath(netman->technologyPathForService(service->path()));
            if (tech.powered()) {
               requestConnect(service->path());
            }
        } else {
            updateServicesMap();
            qDebug() <<"serviceInProgress"<< serviceInProgress <<"currentNetworkState" << currentNetworkState;
            // If a manual connection has recently been detected autoConnect() will do nothing.
            // Always call autoConnect() here as this state change could be that manual connection
            // disconnecting.
            autoConnect();
        }
    }
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

    if (manualConnnectionTimer.isValid() && !manualConnnectionTimer.hasExpired(1 * 60 * 1000)) {
        qDebug() << "skipping auto connect," << (manualConnnectionTimer.elapsed() / 1000)
                 << "seconds since manual connection.";
        return false;
    } else {
        qDebug() << "clearing manual connected service data";
        lastManuallyConnectedService.clear();
        manualConnnectionTimer.invalidate();
    }

    if (selectedService.isEmpty()) {
        selectedService = findBestConnectableService();
    }

    // Don't immediately reconnect if the manual disconnection timer has not expired. This prevents
    // connectionagent from aborting a manual connection initiated by another process.
    if (manualDisconnectionTimer.isValid() && !manualDisconnectionTimer.hasExpired(10000) &&
        lastManuallyDisconnectedService == selectedService) {
        qDebug() << "Don't immediately reconnect if the manual disconnection timer has not expired.";
        return false;
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
    bool needConfig = true;

    if (servicesList.isEmpty()) {
        if (type == "wifi") {
            needConfig = false;
            QObject::connect(&netTech,SIGNAL(scanFinished()),this,SLOT(onScanFinished()));
            netTech.scan();
        }
    } else {
        currentType = "";

        Q_FOREACH (const QString path, servicesList) {
            NetworkService *service = servicesMap.value(path);
            if (service) {
                if (service->favorite() && service->autoConnect()) {
                    needConfig = false;
                    if (!connectToNetworkService(path))
                        continue;

                    lastManuallyConnectedService = serviceInProgress;
                    manualConnnectionTimer.start();
                    break;
                }
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
             << netman->state()
             << serviceInProgress;

    if (!servicesMap.contains(servicePath) || !serviceInProgress.isEmpty())
        return false;

    QString type = servicesMap.value(servicePath)->type();
    if (type.isEmpty() || !netman->getTechnology(type)->powered())
        return false;

    if (servicePath.contains("cellular")) {

        QOfonoManager oManager;
        if (!oManager.available() || oManager.modems().count() < 1) {
            qDebug() << "ofono not available.";
            return false;
        }
        QOfonoNetworkRegistration ofonoReg;
        ofonoReg.setModemPath(oManager.modems().at(0));

        if (ofonoReg.status() != "registered"
                &&  ofonoReg.status() != "roaming") { //not on any cell network so bail
            qDebug() << "ofono is not registered yet";
            return false;
        }

        QOfonoConnectionManager oConnManager;
        oConnManager.setModemPath(oManager.modems().at(0));

        //isCellRoaming
        if (servicesMap.value(servicePath)->roaming()
                && !oConnManager.roamingAllowed()) {
            //roaming and user doesnt want connection while roaming
            qDebug() << "roaming not allowed";
            return false;
        }
    }
    requestConnect(servicePath);
    return true;
}

void QConnectionManager::onScanFinished()
{
    qDebug() << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";
    if (!lastManuallyConnectedService.isEmpty())
        lastManuallyConnectedService.clear();
    if (manualConnnectionTimer.isValid())
        manualConnnectionTimer.invalidate();
    autoConnect();
}

void QConnectionManager::updateServicesMap()
{
    qDebug() << Q_FUNC_INFO;
    QStringList oldServices = orderedServicesList;
    orderedServicesList.clear();

    Q_FOREACH (const QString &tech,techPreferenceList) {
        QVector<NetworkService*> services = netman->getServices(tech);

        Q_FOREACH (NetworkService *serv, services) {
            qDebug() << "known service:" << serv->name() << serv->strength();

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
                QObject::connect(serv, SIGNAL(autoConnectChanged(bool)),
                                 this,SLOT(serviceAutoconnectChanged(bool)), Qt::UniqueConnection);
            }
        }
    }
    qDebug() << orderedServicesList;
}

void QConnectionManager::servicesError(const QString &errorMessage)
{
    if (errorMessage.isEmpty())
        return;
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

        qDebug() << "looking at"
                 << service->name()
                 << service->autoConnect()
                 << lastManuallyConnectedService;

        bool online = isStateOnline(netman->defaultRoute()->state());
        qDebug() << "state is"<< online << netman->defaultRoute()->path();

        if (!service->autoConnect()) {
            continue;
        }

        qDebug() <<Q_FUNC_INFO<< "continued"
                << online
                << (netman->defaultRoute()->path() == service->path())
                << netman->defaultRoute()->strength()
                << service->strength();

        if ((netman->defaultRoute()->type() == "wifi" && service->type() == "wifi")
                &&  netman->defaultRoute()->strength() > service->strength())
            return QString(); //better quality already connected

        if (netman->defaultRoute()->type() == "wifi" && service->type() != "wifi")
            return QString(); // prefer connected wifi

        bool isCellRoaming = false;
        if (service->type() == "cellular" && service->roaming() && !service->connected()) {

            isCellRoaming = askForRoaming;

            QOfonoManager oManager;
            if (!oManager.available()) {
                qDebug() << "ofono not available.";
            }
            if (oManager.modems().count() < 1)
                return QString();

            QOfonoConnectionManager oConnManager;
            oConnManager.setModemPath(oManager.modems().at(0));

            if (oConnManager.roamingAllowed()) {
                if (askRoaming()) {
                    // ask user
                    if (serviceInProgress.isEmpty() && !flightModeSuppression) {
                        Q_EMIT connectionRequest();
                    }
                    return QString();
                }
            }
                //roaming and user doesnt want connection while roaming
                qDebug() << "roaming not allowed";
                return QString();
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

    if ((state == "online" && netman->defaultRoute()->type() == "cellular")
            || (state == "idle" && serviceInProgress.isEmpty())) {
        // on gprs, scan wifi every scanTimeoutInterval minutes
        if (scanTimeoutInterval != 0)
            scanTimer->start(scanTimeoutInterval * 60 * 1000);
    }
}

void QConnectionManager::onServiceStrengthChanged(uint level)
{
    NetworkService *service = static_cast<NetworkService *>(sender());
    if (!wifiStrengths.contains(service->path())) {
        QList <uint> strengthList;
        strengthList << level;
        wifiStrengths.insert(service->path(),strengthList);
    } else {
        QList <uint> strengthList = wifiStrengths[service->path()];
        if (strengthList.count() < 5) {
            strengthList << level;
            wifiStrengths.insert(service->path(),strengthList);

        } else {
            strengthList.removeFirst();
            strengthList << level;
            wifiStrengths.insert(service->path(),strengthList);
        }
    }
    qDebug() << service->name() << level<< averageSignalStrength(service->path());
}

uint QConnectionManager::averageSignalStrength(const QString &servicePath)
{
    if (!wifiStrengths.contains(servicePath) || wifiStrengths[servicePath].isEmpty())
        return 0;
    uint average = 0;
    Q_FOREACH (uint value, wifiStrengths[servicePath]) {
        average += value;
    }
    return average / wifiStrengths[servicePath].count();
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
    if (wifiStrengths.contains(srv))
            wifiStrengths.remove(srv);
}

void QConnectionManager::setup()
{
    qDebug() << Q_FUNC_INFO
             << connmanAvailable;

    if (connmanAvailable) {
        qDebug() << Q_FUNC_INFO
                 << netman->state();

        updateServicesMap();
        offlineModeChanged(netman->offlineMode());

        tetheringWifiTech = netman->getTechnology("wifi");
        QObject::connect(tetheringWifiTech,SIGNAL(scanFinished()),this,SLOT(onScanFinished()));

        techChanged();
        connect(netman,SIGNAL(technologiesChanged()),this,SLOT(techChanged()));

        QSettings confFile;
        confFile.beginGroup("Connectionagent");
        scanTimeoutInterval = confFile.value("scanTimerInterval", "1").toUInt(); //in minutes

        if (isStateOnline(netman->state())) {
            qDebug() << "default route type:" << netman->defaultRoute()->type();
            if (netman->defaultRoute()->type() == "ethernet")
                isEthernet = true;
            if (netman->defaultRoute()->type() == "cellular" && scanTimeoutInterval != 0)
                scanTimer->start(scanTimeoutInterval * 60 * 1000);

        } else
            netman->setSessionMode(true); //turn off connman autoconnecting
                                          // in later versions (1.18) this won't effect autoconnect
                                          // so we will have to turn it off some other way

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
    qDebug() << tech->name() << b;

    if (b && tech->type() == "wifi") {
        tetheringWifiTech->scan();
    }

    lastManuallyDisconnectedService.clear();
    QTimer::singleShot(1000,this,SLOT(delayedAutoconnect()));
}

void QConnectionManager::techChanged()
{
    if (netman->getTechnologies().isEmpty()) {
        knownTechnologies.clear();
    }
    if (!netman->getTechnology("wifi")) {
        tetheringWifiTech = 0;
        return;
    }
    if (tetheringWifiTech) {
        tetheringEnabled = tetheringWifiTech->tethering();
        qDebug() << "tethering is" << tetheringEnabled;
        QObject::connect(tetheringWifiTech, SIGNAL(tetheringChanged(bool)),
                         this,SLOT(techTetheringChanged(bool)), Qt::UniqueConnection);
    }

    Q_FOREACH(NetworkTechnology *technology,netman->getTechnologies()) {
        if (!knownTechnologies.contains(technology->path())) {
            knownTechnologies << technology->path();
            if (technology->type() == "wifi") {
                tetheringWifiTech = technology;
                connect(tetheringWifiTech,SIGNAL(poweredChanged(bool)),this,SLOT(technologyPowerChanged(bool)));
            }
        } else {
            knownTechnologies.removeOne(technology->path());
        }
    }
}

void QConnectionManager::browserRequest(const QString &servicePath, const QString &url)
{
    Q_UNUSED(servicePath)
    qDebug() << servicePath;
    qDebug() << url;
    goodConnectTimer->stop();

    goodConnectTimer->setInterval(5 * 60 * 1000);//may take user up to 5 mintues to figure out the passphrase
    goodConnectTimer->start();

    if (netman->defaultRoute()->type() == "cellular") {
        lastManuallyDisconnectedService = netman->defaultRoute()->path();
        requestDisconnect(netman->defaultRoute()->path());
    }

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
}

bool QConnectionManager::isBestService(const QString &servicePath)
{
    qDebug() << Q_FUNC_INFO
             << servicePath
             << tetheringEnabled
             << netman->defaultRoute()->path().contains(servicePath)
             << (netman->defaultRoute()->state() != "online")
             << netman->defaultRoute()->strength()
             << servicesMap.value(servicePath)->strength();
    qDebug() << lastManuallyDisconnectedService;
    qDebug() << averageSignalStrength(servicePath) << averageSignalStrength( netman->defaultRoute()->path());

    if (tetheringEnabled) return false;
    if (netman->offlineMode() && servicesMap.value(servicePath)->type() == "cellular") return false;
    if (netman->defaultRoute()->type() == "wifi" && servicesMap.value(servicePath)->type() == "cellular") return false;
    if (netman->defaultRoute()->path().contains(servicePath)) return false;
    if (!serviceInProgress.isEmpty() && serviceInProgress != servicePath) return false;
    if ((servicesMap.value(servicePath)->type() == netman->defaultRoute()->type()) //diff tech has diff signal strength
            && netman->defaultRoute()->strength() != 0
            && averageSignalStrength(netman->defaultRoute()->path()) + 4 > averageSignalStrength(servicePath)) return false;
    // 4 is some random number to try and keep connection switching to a minumum
    return true;
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
        qDebug() << servicePath;
        servicesMap.value(servicePath)->requestDisconnect();
        if (scanTimer->isActive())
            scanTimer->stop();
    }
}

void QConnectionManager::requestConnect(const QString &servicePath)
{
    if (servicesMap.contains(servicePath)) {
        qDebug() << servicePath;
         serviceInProgress = servicePath;
         if (netman->defaultRoute()->connected()) {
             delayedConnectService = servicePath;
             requestDisconnect(netman->defaultRoute()->path());
         } else {
             delayedConnectService = servicePath;
             delayedConnect();
         }
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
        if (wifiTech && wifiTech->powered() && !wifiTech->connected()) {
            wifiTech->scan();
        }
    }
}

void QConnectionManager::sleepStateChanged(bool on)
{
    Q_UNUSED(on)
}

void QConnectionManager::goodConnectionTimeout()
{
    qDebug() <<netman->state();
    if (netman->state() != "online") {

        if (servicesMap.contains(serviceInProgress) && servicesMap.value(serviceInProgress)->state() == "ready") {
            delayedConnectService = serviceInProgress;
            requestDisconnect(serviceInProgress);
        }
        errorReported(serviceInProgress,"connection failure");

        if (servicesMap.value(serviceInProgress)->state() == "association"
                && servicesMap.value(serviceInProgress)->type() == "wifi") {
            // connman is stuck in association state, we need to do something drastic to reset
            netman->getTechnology("wifi")->setPowered(false);
            netman->getTechnology("wifi")->setPowered(true);
        }
        serviceInProgress.clear();
    }
}

void QConnectionManager::serviceAutoconnectChanged(bool on)
{
    NetworkService *service = qobject_cast<NetworkService *>(sender());

    qDebug() << service->path() << "AutoConnect is" << on
             << lastManuallyDisconnectedService;
    if (on && service->path() == lastManuallyDisconnectedService) {
        // Auto connect has been changed allow it to
        // be immediately connected.
        manualDisconnectionTimer.invalidate();
        lastManuallyDisconnectedService.clear();
    }
    if (scanTimer->isActive())
        scanTimer->stop();
    autoConnect();
}

void QConnectionManager::scanTimeout()
{
    if (tetheringWifiTech && tetheringWifiTech->powered() && !tetheringWifiTech->connected() && netman->defaultRoute()->type() != "wifi" ) {
        tetheringWifiTech->scan();
        qDebug() << "start scanner" << scanTimeoutInterval;
        if (scanTimeoutInterval != 0) {
            scanTimer->start(scanTimeoutInterval * 60 * 1000);
        }
    }
}

void QConnectionManager::delayedConnect()
{
    qDebug() << "<<<<<<<<<<<<<<<<<<<<<<<<" <<delayedConnectService;
    if (!delayedConnectService.isEmpty()) {
        serviceInProgress = delayedConnectService;
        servicesMap.value(delayedConnectService)->requestConnect();
    }
    delayedConnectService.clear();
}

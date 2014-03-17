/****************************************************************************
**
** Copyright (C) 2014 Jolla Ltd
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

#include "qconnectionagent.h"
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

#define CONNMAN_1_21

QConnectionAgent* QConnectionAgent::self = NULL;

#define CONND_SERVICE "com.jolla.Connectiond"
#define CONND_PATH "/Connectiond"
#define CONND_SESSION_PATH = "/ConnectionSession"

QConnectionAgent::QConnectionAgent(QObject *parent) :
    QObject(parent),
    ua(0),
    netman(NetworkManagerFactory::createInstance()),
    currentNetworkState(QString()),
    askForRoaming(false),
    isEthernet(false),
    connmanAvailable(false),
    oContext(0),
    tetheringWifiTech(0),
    tetheringEnabled(false),
    flightModeSuppression(false),
    scanTimeoutInterval(1)
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

    connect(netman,SIGNAL(servicesListChanged(QStringList)),this,SLOT(servicesListChanged(QStringList)));
    connect(netman,SIGNAL(stateChanged(QString)),this,SLOT(networkStateChanged(QString)));
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
        techPreferenceList << "bluetooth" << "wifi" << "cellular" << "ethernet";

    mceWatch = new WakeupWatcher(this);
    connect(mceWatch,SIGNAL(displayStateChanged(QString)),this,SLOT(displayStateChanged(QString)));

    connmanAvailable = QDBusConnection::systemBus().interface()->isServiceRegistered("net.connman");

    scanTimer = new QTimer(this);
    connect(scanTimer,SIGNAL(timeout()),this,SLOT(scanTimeout()));
    scanTimer->setSingleShot(true);
    if (connmanAvailable)
        setup();
}

QConnectionAgent::~QConnectionAgent()
{
    delete self;
}

QConnectionAgent & QConnectionAgent::instance()
{
    qDebug() << Q_FUNC_INFO;
    if (!self) {
        self = new QConnectionAgent;
    }

    return *self;
}

// from useragent
void QConnectionAgent::onUserInputRequested(const QString &servicePath, const QVariantMap &fields)
{
    qDebug() << servicePath;
    // gets called when a connman service gets called to connect and needs more configurations.
    Q_EMIT userInputRequested(servicePath, fields);
}

// from useragent
void QConnectionAgent::onUserInputCanceled()
{
    qDebug() ;
    Q_EMIT userInputCanceled();
}

// from useragent
void QConnectionAgent::onErrorReported(const QString &servicePath, const QString &error)
{
    qDebug() << "<<<<<<<<<<<<<<<<<<<<" << servicePath << error;
    Q_EMIT errorReported(servicePath, error);
}

// from useragent
void QConnectionAgent::onConnectionRequest()
{
    sendConnectReply("Suppress", 15);
    qDebug() << flightModeSuppression;
    if (!flightModeSuppression) {
        Q_EMIT connectionRequest();
    }
}

void QConnectionAgent::sendConnectReply(const QString &in0, int in1)
{
    ua->sendConnectReply(in0, in1);
}

void QConnectionAgent::sendUserReply(const QVariantMap &input)
{
    qDebug() << Q_FUNC_INFO;
    ua->sendUserReply(input);
}

void QConnectionAgent::servicesListChanged(const QStringList &list)
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
}

void QConnectionAgent::serviceErrorChanged(const QString &error)
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

void QConnectionAgent::serviceStateChanged(const QString &state)
{
    NetworkService *service = static_cast<NetworkService *>(sender());
    qDebug() << state << service->name() << service->strength();
    qDebug() << "currentNetworkState" << currentNetworkState;

    if (!service->favorite() || !netman->getTechnology(service->type())->powered()) {
        qDebug() << "not fav or not powered";
        return;
    }
    if (state == "disconnect") {
        ua->sendConnectReply("Clear");
    }
//    if (state == "failure") {
//        serviceInProgress.clear();
//     //   service->requestDisconnect();
//    }

    if (state == "online") {
        Q_EMIT connectionState(state, service->type());
    }

    //auto migrate
    if (state == "idle") {
        } else {
        updateServicesMap();
    }
    currentNetworkState = state;
    QSettings confFile;
    confFile.beginGroup("Connectionagent");
    confFile.setValue("connected",currentNetworkState);
}

// from plugin/qml
void QConnectionAgent::connectToType(const QString &type)
{
    qDebug() << type;
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
        Q_FOREACH (const QString path, servicesList) {
            NetworkService *service = servicesMap.value(path);
            if (service) {
                if (service->favorite() && service->autoConnect()) {
                    needConfig = false;
                    service->requestConnect();
                    break;
                }
            }
        }
    }
    if (needConfig) {
        Q_EMIT configurationNeeded(type);
    }
}

void QConnectionAgent::onScanFinished()
{
    qDebug() << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>";
}

void QConnectionAgent::updateServicesMap()
{
    qDebug() << Q_FUNC_INFO;
    QStringList oldServices = orderedServicesList;
    orderedServicesList.clear();

    Q_FOREACH (const QString &tech,techPreferenceList) {
        QVector<NetworkService*> services = netman->getServices(tech);

        Q_FOREACH (NetworkService *serv, services) {
            const QString servicePath = serv->path();

            qDebug() << "known service:" << serv->name() << serv->strength();

            servicesMap.insert(servicePath, serv);
            orderedServicesList << servicePath;

            if (!oldServices.contains(servicePath)) {
                //new!
                qDebug() <<"new service"
                         << servicePath;

                QObject::connect(serv, SIGNAL(stateChanged(QString)),
                                 this,SLOT(serviceStateChanged(QString)), Qt::UniqueConnection);
                QObject::connect(serv, SIGNAL(connectRequestFailed(QString)),
                                 this,SLOT(serviceErrorChanged(QString)), Qt::UniqueConnection);

                QObject::connect(serv, SIGNAL(errorChanged(QString)),
                                 this,SLOT(servicesError(QString)), Qt::UniqueConnection);

                QObject::connect(serv, SIGNAL(autoConnectChanged(bool)),
                                 this,SLOT(serviceAutoconnectChanged(bool)), Qt::UniqueConnection);
            }
        }
    }

    qDebug() << orderedServicesList;
}

void QConnectionAgent::servicesError(const QString &errorMessage)
{
    if (errorMessage.isEmpty())
        return;
    NetworkService *serv = static_cast<NetworkService *>(sender());
    qDebug() << serv->name() << errorMessage;
    Q_EMIT onErrorReported(serv->path(), errorMessage);
}

void QConnectionAgent::ofonoServicesError(const QString &errorMessage)
{
    QOfonoConnectionContext *context = static_cast<QOfonoConnectionContext *>(sender());
    QVector<NetworkService*> services = netman->getServices("cellular");
    Q_FOREACH (NetworkService *serv, services) {
        if (context->contextPath().contains(serv->path().section("_",2,2))) {
            Q_EMIT onErrorReported(serv->path(), errorMessage);
            qDebug() << serv->name() << errorMessage;
            return;
        }
    }
    qWarning() << "ofono error but could not discover connman service";
}

void QConnectionAgent::networkStateChanged(const QString &state)
{
    qDebug() << state;

    QSettings confFile;
    confFile.beginGroup("Connectionagent");
    if (state != "online")
        confFile.setValue("connected","offline");
    else
        confFile.setValue("connected","online");

    if ((state == "online" && netman->defaultRoute()->type() == "cellular")
            || (state == "idle")) {
        // on gprs, scan wifi every scanTimeoutInterval minutes
        if (scanTimeoutInterval != 0)
            scanTimer->start(scanTimeoutInterval * 60 * 1000);
    }
}

bool QConnectionAgent::askRoaming() const
{
    qDebug() << Q_FUNC_INFO;
    bool roaming;
    QSettings confFile;
    confFile.beginGroup("Connectionagent");
    roaming = confFile.value("askForRoaming").toBool();
    return roaming;
}

void QConnectionAgent::setAskRoaming(bool value)
{
    qDebug() << Q_FUNC_INFO;
    QSettings confFile;
    confFile.beginGroup("Connectionagent");
    confFile.setValue("askForRoaming",value);
    askForRoaming = value;
}

void QConnectionAgent::connmanAvailabilityChanged(bool b)
{
    qDebug() << Q_FUNC_INFO;
    connmanAvailable = b;
    if (b) {
        setup();
        currentNetworkState = netman->state();
    } else {
        servicesMap.clear();
        currentNetworkState = "error";
    }
}

void QConnectionAgent::serviceAdded(const QString &srv)
{
    qDebug() << Q_FUNC_INFO << "<<<<"<< srv;
    updateServicesMap();
}

void QConnectionAgent::serviceRemoved(const QString &srv)
{
    qDebug() << Q_FUNC_INFO << "<<<<" << srv;
    if (orderedServicesList.contains(srv)) {
        orderedServicesList.removeOne(srv);
    }
}

void QConnectionAgent::setup()
{
    qDebug() << Q_FUNC_INFO
             << connmanAvailable;

    if (connmanAvailable) {
        qDebug() << Q_FUNC_INFO
                 << netman->state();
        if (ua)
            delete ua;

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

        }
        qDebug() << "config file says" << confFile.value("connected", "online").toString();
    }
}

void QConnectionAgent::technologyPowerChanged(bool b)
{
    NetworkTechnology *tech = static_cast<NetworkTechnology *>(sender());
    qDebug() << tech->name() << b;

    if (b && tech->type() == "wifi") {
        tetheringWifiTech->scan();
    }
}

void QConnectionAgent::techChanged()
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

void QConnectionAgent::browserRequest(const QString &servicePath, const QString &url)
{
    Q_UNUSED(servicePath)
    qDebug() << servicePath;
    qDebug() << url;

    Q_EMIT requestBrowser(url);
}

bool QConnectionAgent::isStateOnline(const QString &state)
{
    if (state == "online" || state == "ready")
        return true;
    return false;
}

void QConnectionAgent::offlineModeChanged(bool b)
{
    flightModeSuppression = b;
    if (b) {
        QTimer::singleShot(5 * 1000 * 60,this,SLOT(flightModeDialogSuppressionTimeout())); //5 minutes
    }
}

void QConnectionAgent::flightModeDialogSuppressionTimeout()
{
    if (flightModeSuppression)
        flightModeSuppression = false;
}

void QConnectionAgent::displayStateChanged(const QString &state)
{
    if (state == "on") {
        NetworkTechnology *wifiTech = netman->getTechnology("wifi");
        if (wifiTech && wifiTech->powered() && !wifiTech->connected()) {
            wifiTech->scan();
        }
    }
}

void QConnectionAgent::serviceAutoconnectChanged(bool on)
{
    NetworkService *service = qobject_cast<NetworkService *>(sender());

    qDebug() << service->path() << "AutoConnect is" << on;
    if (!on) {
        servicesMap.value(service->path())->requestDisconnect();
    } else {
        servicesMap.value(service->path())->requestConnect();
    }
}

void QConnectionAgent::scanTimeout()
{
    if (tetheringWifiTech && tetheringWifiTech->powered() && !tetheringWifiTech->connected() && netman->defaultRoute()->type() != "wifi" ) {
        tetheringWifiTech->scan();
        qDebug() << "start scanner" << scanTimeoutInterval;
        if (scanTimeoutInterval != 0) {
            scanTimer->start(scanTimeoutInterval * 60 * 1000);
        }
    }
}

/****************************************************************************
**
** Copyright (C) 2012 Jolla Ltd.
** Contact: lorn.potter@gmail.com
**

**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
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
     netman(NetworkManagerFactory::createInstance())
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

    qDebug() << Q_FUNC_INFO;// << servicePath << fields;
    Q_EMIT userInputRequested(servicePath, fields);
}

// from useragent
void QConnectionManager::onUserInputCanceled()
{
    qDebug() << Q_FUNC_INFO;
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
//    if (!autoConnect()) {
        Q_EMIT connectionRequest();

  //  }
}

void QConnectionManager::sendMessage()
{
  //  ua->sendConnectReply(QLatin1String("Suppress"));
}

void QConnectionManager::sendConnectReply(const QString &in0, int in1)
{
    qDebug() << Q_FUNC_INFO << in0 << in1;
    ua->sendConnectReply(in0, in1);
}

void QConnectionManager::sendUserReply(const QVariantMap &input)
{
    qDebug() << Q_FUNC_INFO;
    ua->sendUserReply(input);
}

void QConnectionManager::networkStateChanged(const QString &state)
{
    QString msg;
    if (currentNetworkState == "idle" && state == "association") {
        msg = "Connecting...";
    } else  if (currentNetworkState == "ready" && state == "online") {
        msg = "Connected";
    } else  if (state == "offline") {
        msg = "Offline";
    }
    qDebug() << Q_FUNC_INFO << currentNetworkState << state << msg;

//    NotificationManager *manager = NotificationManager::instance();
//    QVariantHash hints;
//    hints.insert(NotificationManager::HINT_URGENCY, 2);
//    hints.insert(NotificationManager::HINT_CATEGORY, "device.error");
//    hints.insert(NotificationManager::HINT_PREVIEW_BODY, msg);
//    manager->Notify(qApp->applicationName(), 0, QString(), QString(), QString(), QStringList(), hints, -1);

//    if (!msg.isEmpty()) {
//        emit serviceStateChanged(msg);
//    }
    currentNetworkState = state;
}

void QConnectionManager::onServiceAdded(const QString &servicePath)
{
//    qDebug() << Q_FUNC_INFO << servicePath;
}

void QConnectionManager::serviceErrorChanged(const QString &error)
{
    qDebug() << Q_FUNC_INFO << error;
//    NotificationManager *manager = NotificationManager::instance();
//    QVariantHash hints;
//    hints.insert(NotificationManager::HINT_URGENCY, 1);
//    hints.insert(NotificationManager::HINT_CATEGORY, "device.error");
//    hints.insert(NotificationManager::HINT_PREVIEW_BODY, error);
//    manager->Notify(qApp->applicationName(), 0, QString(), QString(), QString(), QStringList(), hints, -1);
}

void QConnectionManager::stateChanged(const QString &state)
{
    qDebug()  << Q_FUNC_INFO << state;
}

bool QConnectionManager::autoConnect()
{
    qDebug() << Q_FUNC_INFO;
    QStringList techList = netman->technologiesList();
    Q_FOREACH (const QString &tech, techList) {

        QVector<NetworkService*> serviceList = netman->getServices(tech);

        Q_FOREACH (NetworkService *service, serviceList) {
            if(service->autoConnect() && service->favorite()) {

                QObject::connect(service, SIGNAL(stateChanged(QString)),
                                 this,SLOT(stateChanged(QString)));
                QObject::connect(service, SIGNAL(connectRequestFailed(QString)),
                                 this,SLOT(serviceErrorChanged(QString)));

                service->requestConnect();
                return true;
            }
        }
    }
    return false;
}


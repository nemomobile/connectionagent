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

#ifndef QCONNECTIONMANAGER_H
#define QCONNECTIONMANAGER_H

#include <QObject>
#include <QMap>
#include <QStringList>
#include <QVariant>
#include <QDBusMessage>
#include <QDBusObjectPath>

class UserAgent;
class SessionAgent;

class ConnAdaptor;
class NetworkManager;
class NetworkService;

class QConnectionManager : public QObject
{
    Q_OBJECT

public:
    ~QConnectionManager();
    
    static QConnectionManager &instance();

Q_SIGNALS:
   void connectionChanged(const QString &, bool); // ?
   void connected(); // ?

    void userInputRequested(const QString &servicePath, const QVariantMap &fields);
    void userInputCanceled();
    void errorReported(const QString &error);
    void connectionRequest();
    void wlanConfigurationNeeded();
    void connectionState(const QString &state, const QString &type);

public Q_SLOTS:

    void onUserInputRequested(const QString &servicePath, const QVariantMap &fields);
    void onUserInputCanceled();
    void onErrorReported(const QString &error);

    void onConnectionRequest();

    void sendMessage();// ?

    void sendConnectReply(const QString &in0, int in1);
    void sendUserReply(const QVariantMap &input);

    void onServiceAdded(const QString &servicePath);
    void onServiceRemoved(const QString &servicePath);
    void serviceErrorChanged(const QString &error);
    void stateChanged(const QString &state);

    void connectToType(const QString &type);
    void defaultRouteChanged(NetworkService* defaultRoute);

private:
    explicit QConnectionManager(QObject *parent = 0);
    static QConnectionManager *self;
    ConnAdaptor *connectionAdaptor;
    UserAgent *ua;

    bool autoConnect();
    NetworkManager *netman;
 //   NetworkService *netService;
    SessionAgent *sessionAgent;

    bool okToConnect;
    QString currentNetworkState;
    QString currentType;
    bool serviceConnect;
    void connectToNetworkService(const QString &service);
    void showNotification(const QString &message,int timeout);
    uint currentNotification;

    QList <QString> servicePaths;
    QHash<QString,NetworkService *> servicesMap;

private slots:
    void onScanFinished();
    void updateServicesMap();
    void sessionSettingsUpdated(const QVariantMap &map);

};

#endif // QCONNECTIONMANAGER_H

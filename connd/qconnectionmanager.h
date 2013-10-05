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
#include <QQueue>
#include <QPair>

class UserAgent;
class SessionAgent;

class ConnAdaptor;
class NetworkManager;
class NetworkService;
class QOfonoConnectionContext;

class QConnectionManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool askRoaming READ askRoaming WRITE setAskRoaming)

public:
    ~QConnectionManager();
    
    static QConnectionManager &instance();
    bool askRoaming() const;
    void setAskRoaming(bool value);

Q_SIGNALS:

    void userInputRequested(const QString &servicePath, const QVariantMap &fields);
    void userInputCanceled();
    void errorReported(const QString &servicePath, const QString &error);
    void connectionRequest();
    void configurationNeeded(const QString &type);
    void connectionState(const QString &state, const QString &type);
    void connectNow(const QString &path);

    void requestBrowser(/*const QString &path,*/ const QString &url);

public Q_SLOTS:

    void onUserInputRequested(const QString &servicePath, const QVariantMap &fields);
    void onUserInputCanceled();
    void onErrorReported(const QString &servicePath, const QString &error);

    void onConnectionRequest();

    void sendConnectReply(const QString &in0, int in1);
    void sendUserReply(const QVariantMap &input);

    void connectToType(const QString &type);

private:
    explicit QConnectionManager(QObject *parent = 0);
    static QConnectionManager *self;
    ConnAdaptor *connectionAdaptor;
    UserAgent *ua;

    bool autoConnect();
    NetworkManager *netman;
    SessionAgent *sessionAgent;

    QString currentNetworkState;
    QString currentType;
    bool serviceConnect;
    uint currentNotification;

    QMap<QString,NetworkService *> servicesMap;
    QStringList orderedServicesList;

    QString findBestConnectableService();
    void connectionHandover(const QString &oldService, const QString &newService);
    QList <QString> connectedServices;
    QStringList techPreferenceList;
    bool askForRoaming;
    bool isEthernet;
    bool connmanAvailable;
    bool handoverInProgress;
    QString lastConnectedService;
    bool manualConnected;
    QString manuallyConnectedService;
    QString serviceInProgress;
    QString autoConnectService;
    QString manuallyDisconnectedService;

    bool isBestService(const QString &servicePath);
    bool isStateOnline(const QString &state);
    void requestDisconnect(const QString &service);
    void requestConnect(const QString &service);
    QOfonoConnectionContext *oContext;

private slots:
    void onScanFinished();
    void updateServicesMap();

    void onServicesChanged();
    void serviceErrorChanged(const QString &error);
    void serviceStateChanged(const QString &state);
    void networkStateChanged(const QString &state);
    void onServiceStrengthChanged(uint);
    void connectToNetworkService(const QString &service);

    void connmanAvailabilityChanged(bool b);
    void setup();
    void emitConnectionState();
    void servicesError(const QString &);
    void ofonoServicesError(const QString &);
    void technologyPowerChanged(bool);
    void browserRequest(const QString &servicePath, const QString &url);
    void onServiceConnectionStarted();

    void onServiceDisconnectionStarted();
    void connectToContext(const QString &servicePath);
};

#endif // QCONNECTIONMANAGER_H

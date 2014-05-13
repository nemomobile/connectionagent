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

#ifndef QCONNECTIONAGENT_H
#define QCONNECTIONAGENT_H

#include <QObject>
#include <QMap>
#include <QStringList>
#include <QVariant>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QQueue>
#include <QPair>
#include <QElapsedTimer>

class UserAgent;
class SessionAgent;

class ConnAdaptor;
class NetworkManager;
class NetworkService;
class QOfonoConnectionContext;
class NetworkTechnology;
class WakeupWatcher;
class QTimer;

class QConnectionAgent : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool askRoaming READ askRoaming WRITE setAskRoaming)

public:
    ~QConnectionAgent();

    static QConnectionAgent &instance();
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

    void requestBrowser(const QString &url);
    void tetheringFinished(bool);

public Q_SLOTS:

    void onUserInputRequested(const QString &servicePath, const QVariantMap &fields);
    void onUserInputCanceled();
    void onErrorReported(const QString &servicePath, const QString &error);

    void onConnectionRequest();

    void sendConnectReply(const QString &in0, int in1);
    void sendUserReply(const QVariantMap &input);

    void connectToType(const QString &type);

    void startTethering(const QString &type);
    void stopTethering();
    void setTetheringSsid(const QString &ssid);
    void setTetheringPassphrase(const QString &passphrase);

private:
    explicit QConnectionAgent(QObject *parent = 0);
    static QConnectionAgent *self;
    ConnAdaptor *connectionAdaptor;
    UserAgent *ua;

    NetworkManager *netman;
    SessionAgent *sessionAgent;

    QString currentNetworkState;

    QMap<QString,NetworkService *> servicesMap;
    QStringList orderedServicesList;

    QStringList techPreferenceList;
    bool askForRoaming;
    bool isEthernet;
    bool connmanAvailable;

    bool isStateOnline(const QString &state);
    QOfonoConnectionContext *oContext;
    NetworkTechnology *tetheringWifiTech;
    bool tetheringEnabled;
    bool flightModeSuppression;
    WakeupWatcher *mceWatch;
    uint scanTimeoutInterval;

    QTimer *scanTimer;
    QStringList knownTechnologies;
    bool isBestService(NetworkService *service);
    QString findBestConnectableService();
    void removeAllTypes(const QString &type);
    bool tetheringStarted;
    bool delayedTethering;

private slots:
    void onScanFinished();
    void updateServicesMap();

    void serviceErrorChanged(const QString &error);
    void serviceStateChanged(const QString &state);
    void networkStateChanged(const QString &state);

    void connmanAvailabilityChanged(bool b);
    void setup();
    void servicesError(const QString &);
    void ofonoServicesError(const QString &);
    void technologyPowerChanged(bool);
    void browserRequest(const QString &servicePath, const QString &url);
    void techChanged();

    void serviceRemoved(const QString &);
    void serviceAdded(const QString &);
    void servicesListChanged(const QStringList &);
    void offlineModeChanged(bool);
    void flightModeDialogSuppressionTimeout();

    void displayStateChanged(const QString &);
//    void sleepStateChanged(bool);

    void serviceAutoconnectChanged(bool);
    void scanTimeout();
    void techTetheringChanged(bool b);
    void servicesChanged();

    void openConnectionDialog(const QString &type);
};

#endif // QCONNECTIONAGENT_H

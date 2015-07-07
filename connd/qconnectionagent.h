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
#include <QVector>

class UserAgent;
class SessionAgent;

class NetworkManager;
class NetworkService;
class NetworkTechnology;
class WakeupWatcher;
class QTimer;

class QConnectionAgent : public QObject
{
    Q_OBJECT

public:
    explicit QConnectionAgent(QObject *parent = 0);
    ~QConnectionAgent();

    bool isValid() const;

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
    void stopTethering(bool keepPowered = false);

private:

    class Service
    {
    public:
            QString path;
            NetworkService *service;

            bool operator==(const Service &other) const {
                    return other.path == path;
            }
    };

    class ServiceList : public QVector<Service>
    {
    public:
            int indexOf(const QString &path, int from = 0) const {
                    Service key;
                    key.path = path;
                    return QVector<Service>::indexOf(key, from);
            }

            bool contains(const QString &path) const {
                    Service key;
                    key.path = path;
                    return QVector<Service>::indexOf(key) >= 0;
            }

            void remove(const QString &path) {
                    Service key;
                    key.path = path;
                    int pos = QVector<Service>::indexOf(key);
                    if (pos >= 0)
                            QVector<Service>::remove(pos);
            }
    };

    void setup();
    void updateServices();
    bool isStateOnline(const QString &state);
    bool isBestService(NetworkService *service);
    QString findBestConnectableService();
    void removeAllTypes(const QString &type);

    UserAgent *ua;

    NetworkManager *netman;
    SessionAgent *sessionAgent;

    QString currentNetworkState;

    ServiceList orderedServicesList;

    QStringList techPreferenceList;
    bool isEthernet;
    bool connmanAvailable;

    NetworkTechnology *tetheringWifiTech;
    bool tetheringEnabled;
    bool flightModeSuppression;
    WakeupWatcher *mceWatch;
    uint scanTimeoutInterval;

    QTimer *scanTimer;
    QStringList knownTechnologies;
    bool tetheringStarted;
    bool delayedTethering;
    bool valid;

private slots:
    void onScanFinished();

    void serviceErrorChanged(const QString &error);
    void serviceStateChanged(const QString &state);
    void networkStateChanged(const QString &state);

    void connmanAvailabilityChanged(bool b);
    void servicesError(const QString &);
    void technologyPowerChanged(bool);
    void browserRequest(const QString &servicePath, const QString &url);
    void techChanged();

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
    void setWifiTetheringEnabled();
};

#endif // QCONNECTIONAGENT_H

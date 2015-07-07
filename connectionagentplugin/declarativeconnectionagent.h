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

#ifndef DECLARATIVECONNECTIONAGENT_H
#define DECLARATIVECONNECTIONAGENT_H

#include "declarativeconnectionagent.h"
#include "connectiond_interface.h"

/*
 *This class is for accessing connman's UserAgent from multiple sources.
 *This is because currently, there can only be one UserAgent per system.
 *
 *It also makes use of a patch to connman, that allows the UserAgent
 *to get signaled when a connection is needed. This is the real reason
 *this daemon is needed. An InputRequest is short lived, and thus, may
 *not clash with other apps that need to use UserAgent.
 *
 *When you are trying to intercept a connection request, you need a long
 *living process to wait until such time. This will immediately clash if
 *a wlan needs user Input signal from connman, and the configure will never
 *get the proper signal.
 *
 *This qml type can be used as such:
 *
 *import com.jolla.connection 1.0
 *
 *    ConnectionAgent {
 *       id: userAgent
 *        onUserInputRequested: {
 *            console.log("        onUserInputRequested:")
 *        }
 *
 *       onConnectionRequest: {
 *          console.log("onConnectionRequest ")
 *            sendSuppress()
 *        }
 *        onErrorReported: {
 *            console.log("Got error from connman: " + error);
 *       }
 *   }
 *
 **/

class DeclarativeConnectionAgent : public QObject
{
    Q_OBJECT

    Q_DISABLE_COPY(DeclarativeConnectionAgent)

public:
    explicit DeclarativeConnectionAgent(QObject *parent = 0);
    ~DeclarativeConnectionAgent();

public slots:
    void sendUserReply(const QVariantMap &input);
    void sendConnectReply(const QString &replyMessage, int timeout = 120);
    void connectToType(const QString &type);
    void startTethering(const QString &type);
    void stopTethering(bool keepPowered = false);

signals:
    void userInputRequested(const QString &servicePath, const QVariantMap &fields);
    void userInputCanceled();
    void errorReported(const QString &servicePath, const QString &error);
    void connectionRequest();
    void configurationNeeded(const QString &type);
    void connectionState(const QString &state, const QString &type);
    void browserRequested(const QString &url);
    void tetheringFinished(bool);

private:
    com::jolla::Connectiond *connManagerInterface;
    QDBusServiceWatcher *connectiondWatcher;

private slots:
    void onErrorReported(const QString &servicePath, const QString &error);
    void onRequestBrowser(const QString &url);
    void onUserInputRequested(const QString &service, const QVariantMap &fields);
    void onConnectionRequested();
    void onConnectionState(const QString &state, const QString &type);
    void onTetheringFinished(bool);

    void connectToConnectiond(const QString = QString());
    void connectiondUnregistered(const QString = QString());
};

#endif


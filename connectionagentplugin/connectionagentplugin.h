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

#ifndef CONNECTIONAGENTPLUGIN_H
#define CONNECTIONAGENTPLUGIN_H

#include "connectionagentplugin.h"
#include "connectionamanagerinterface.h"

class NetworkManager;
class ConnectionAgentPlugin : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool askRoaming READ askRoaming WRITE setAskRoaming)

    Q_DISABLE_COPY(ConnectionAgentPlugin)
    
public:
    explicit ConnectionAgentPlugin(QObject *parent = 0);
    ~ConnectionAgentPlugin();
    bool askRoaming() const;
    void setAskRoaming(bool value);

public slots:
    void sendUserReply(const QVariantMap &input);
    void sendConnectReply(const QString &replyMessage, int timeout = 120);
    void connectToType(const QString &type);

signals:
    void userInputRequested(const QString &servicePath, const QVariantMap &fields);
    void userInputCanceled();
    void errorReported(const QString &servicePath, const QString &error);
    void connectionRequest();
    void configurationNeeded(const QString &type);
    void connectionState(const QString &state, const QString &type);

private:
    com::jolla::Connectiond *connManagerInterface;
    QDBusServiceWatcher *connectiondWatcher;

private slots:
    void onErrorReported(const QString &servicePath, const QString &error);
    void onRequestBrowser(const QString &url);
    void onUserInputRequested(const QString &service, const QVariantMap &fields);
    void onConnectionRequested();
    void onConnectionState(const QString &state, const QString &type);

    void connectToConnectiond(const QString = QString());
    void connectiondUnregistered(const QString = QString());
};

#endif // CONNECTIONAGENTPLUGIN_H


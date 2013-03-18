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

public Q_SLOTS:

    void onUserInputRequested(const QString &servicePath, const QVariantMap &fields);
    void onUserInputCanceled();
    void onErrorReported(const QString &error);

    void onConnectionRequest();

    void sendMessage();// ?

    void sendConnectReply(const QString &in0, int in1);
    void sendUserReply(const QVariantMap &input);

    void networkStateChanged(const QString &state);
    void onServiceAdded(const QString &servicePath);
    void serviceErrorChanged(const QString &error);
    void stateChanged(const QString &state);

private:
    explicit QConnectionManager(QObject *parent = 0);
    static QConnectionManager *self;
    ConnAdaptor *connectionAdaptor;
    UserAgent *ua;
    NetworkManager *netman;
    bool autoConnect();
    QString currentNetworkState;
};

#endif // QCONNECTIONMANAGER_H

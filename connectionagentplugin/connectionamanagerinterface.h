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


#ifndef CONNECTIONAMANAGERINTERFACE_H_1363426405
#define CONNECTIONAMANAGERINTERFACE_H_1363426405

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtDBus/QtDBus>

/*
 * Proxy class for interface com.jolla.Connectiond
 */
class ConnectionManagerInterface: public QDBusAbstractInterface
{
    Q_OBJECT
public:
    static inline const char *staticInterfaceName()
    { return "com.jolla.Connectiond"; }

public:
    ConnectionManagerInterface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent = 0);

    ~ConnectionManagerInterface();

public Q_SLOTS: // METHODS
    inline QDBusPendingReply<> sendConnectReply(const QString &in0, int in1)
    {
        QList<QVariant> argumentList;
        argumentList << QVariant::fromValue(in0) << QVariant::fromValue(in1);
        return asyncCallWithArgumentList(QLatin1String("sendConnectReply"), argumentList);
    }

    inline QDBusPendingReply<> sendUserReply(const QVariantMap &input)
    {
        qDebug() << Q_FUNC_INFO;
        QList<QVariant> argumentList;
        argumentList << QVariant::fromValue(input);
        return asyncCallWithArgumentList(QLatin1String("sendUserReply"), argumentList);
    }

Q_SIGNALS: // SIGNALS
    void connectionRequest();
    void errorReported(const QString &error);
    void requestBrowser(const QString &url);
    void userInputCanceled();
    void userInputRequested(const QString &service, const QVariantMap &fields);
};

namespace com {
  namespace jolla {
    typedef ::ConnectionManagerInterface Connectiond;
  }
}
#endif

/****************************************************************************
**
** Copyright (C) 2013 Jolla Ltd
** Contact: lorn.potter@jollamobile.com
**
**
** $QT_BEGIN_LICENSE:LGPL$
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QString>
#include <QtTest>
#include <QProcess>

#include "../../../connd/qconnectionagent.h"

#include <networkmanager.h>
#include <networktechnology.h>
#include <networkservice.h>


class Tst_connectionagent : public QObject
{
    Q_OBJECT

public:
    Tst_connectionagent();

private Q_SLOTS:
    void tst_networkInstance();
    void tst_restartconnman();

    void tst_onUserInputRequested();
    void tst_onUserInputCanceled();
    void tst_onErrorReported();
    void tst_onConnectionRequest();
    void tst_sendConnectReply();
    void tst_sendUserReply();
    void tst_connectToType();
};

Tst_connectionagent::Tst_connectionagent()
{
}

void Tst_connectionagent::tst_networkInstance()
{
    NetworkManager *netman = NetworkManagerFactory::createInstance();
    QString currentState = netman->state();
    QConnectionAgent::instance();
    QVERIFY(currentState == netman->state());
}

void Tst_connectionagent::tst_restartconnman()
{
}

void Tst_connectionagent::tst_onUserInputRequested()
{
    QSignalSpy spy(&QConnectionAgent::instance(), SIGNAL(userInputRequested(QString,QVariantMap)));
    QVariantMap map;
    map.insert("test",true);

    QConnectionAgent::instance().onUserInputRequested(QLatin1String("test_path"), map);
    QCOMPARE(spy.count(),1);
    QList<QVariant> arguments;
    arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QString("test_path"));
    QVariantMap map2 = arguments.at(1).toMap();
    QCOMPARE(map2.keys().at(0), QString("test"));

}

void Tst_connectionagent::tst_onUserInputCanceled()
{
    QSignalSpy spy(&QConnectionAgent::instance(), SIGNAL(userInputCanceled()));
    QConnectionAgent::instance().onUserInputCanceled();
    QCOMPARE(spy.count(),1);
}

void Tst_connectionagent::tst_onErrorReported()
{
    QSignalSpy spy(&QConnectionAgent::instance(), SIGNAL(errorReported(QString,QString)));
    QConnectionAgent::instance().onErrorReported("test_path","Test error");

    QCOMPARE(spy.count(),1);
    QList<QVariant> arguments;
    arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QString("test_path"));
    QCOMPARE(arguments.at(1).toString(), QString("Test error"));

    QConnectionAgent::instance().connectToType("test");
    QCOMPARE(spy.count(),1);
    arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QString(""));
    QCOMPARE(arguments.at(1).toString(), QString("Type not valid"));

}

void Tst_connectionagent::tst_onConnectionRequest()
{
    NetworkManager *netman = NetworkManagerFactory::createInstance();
    QString currentState = netman->state();
    if (currentState == "online") {
        NetworkService *service = netman->defaultRoute();
        service->requestDisconnect();
//        service->requestConnect();
    }
    QSignalSpy spy(&QConnectionAgent::instance(), SIGNAL(connectionRequest()));
    QConnectionAgent::instance().onConnectionRequest();

    if (currentState == "online")
        QCOMPARE(spy.count(),0);
    else
        QCOMPARE(spy.count(),0);

}

void Tst_connectionagent::tst_sendConnectReply()
{
}

void Tst_connectionagent::tst_sendUserReply()
{
}

void Tst_connectionagent::tst_connectToType()
{
}


QTEST_APPLESS_MAIN(Tst_connectionagent)

#include "tst_connectionagent.moc"

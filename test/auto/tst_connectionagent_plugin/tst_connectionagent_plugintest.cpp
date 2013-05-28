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
#include "../../../connectionagentplugin/connectionagentplugin.h"

#include <networkmanager.h>
#include <networktechnology.h>
#include <networkservice.h>

#include <QNetworkAccessManager>
#include <QNetworkRequest>


class Tst_connectionagent_pluginTest : public QObject
{
    Q_OBJECT
    
public:
    Tst_connectionagent_pluginTest();
    ~Tst_connectionagent_pluginTest();

private Q_SLOTS:
    void testRequestConnection_data();
    void testRequestConnection();

//    void testError_data();
    void testError();
private:
    ConnectionAgentPlugin *plugin;
};

Tst_connectionagent_pluginTest::Tst_connectionagent_pluginTest()
{
    plugin = new ConnectionAgentPlugin(this);
}

Tst_connectionagent_pluginTest::~Tst_connectionagent_pluginTest()
{
}

void Tst_connectionagent_pluginTest::testRequestConnection_data()
{
    QTest::addColumn<QString>("tech");
    QTest::newRow("wifi") << "wifi";
    QTest::newRow("cellular") << "cellular";
}

void Tst_connectionagent_pluginTest::testRequestConnection()
{
    QFETCH(QString, tech);
    NetworkManager *netman = NetworkManagerFactory::createInstance();

    QString techPath = netman->technologyPathForType(tech);
    NetworkTechnology netTech;
    netTech.setPath(techPath);

    QVector<NetworkService*> wifiServices = netman->getServices("wifi");
    for (int i = 0; i < wifiServices.count(); i++) {
//        favorite = wifiServices[i]->favorite();
        if (wifiServices[i]->state() == "online"
                || wifiServices[i]->state() == "ready") {
            wifiServices[i]->requestDisconnect();
        }
    }

    QSignalSpy spy2(plugin, SIGNAL(connectionRequest()));

    QNetworkAccessManager manager;
    manager.get(QNetworkRequest(QUrl("http://jolla.com")));

    QTest::qWait(4000);
    QCOMPARE(spy2.count(),1);
    plugin->sendConnectReply("Clear",0);


}

//void Tst_connectionagent_pluginTest::testError_data()
//{
//}

void Tst_connectionagent_pluginTest::testError()
{
    QSignalSpy spy(plugin, SIGNAL(errorReported(QString)));
    plugin->connectToType("test");
    QTest::qWait(2000);
    QCOMPARE(spy.count(),1);
    QList<QVariant> arguments = spy.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QString("Type not valid"));
}

QTEST_MAIN(Tst_connectionagent_pluginTest)

#include "tst_connectionagent_plugintest.moc"

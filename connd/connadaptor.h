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

#ifndef CONNADAPTOR_H_1363412350
#define CONNADAPTOR_H_1363412350

#include <QtCore/QObject>
#include <QtDBus/QtDBus>
class QByteArray;
template<class T> class QList;
template<class Key, class Value> class QMap;
class QString;
class QStringList;
class QVariant;

/*
 * Adaptor class for interface com.jolla.Connectiond
 */
class ConnAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.jolla.Connectiond")
    Q_CLASSINFO("D-Bus Introspection", ""
"  <interface name=\"com.jolla.Connectiond\">\n"
"    <method name=\"sendUserReply\">\n"
"      <annotation value=\"QVariantMap\" name=\"org.qtproject.QtDBus.QtTypeName.In0\"/>\n"
"      <arg type=\"a{sv}\" name=\"input\"/>\n"
"    </method>\n"
"    <method name=\"sendConnectReply\">\n"
"      <arg direction=\"in\" type=\"s\"/>\n"
"      <arg direction=\"in\" type=\"i\"/>\n"
"    </method>\n"
"    <signal name=\"userInputRequested\">\n"
"      <annotation value=\"QVariantMap\" name=\"org.qtproject.QtDBus.QtTypeName.In1\"/>\n"
"      <arg type=\"s\" name=\"service\"/>\n"
"      <arg type=\"a{sv}\" name=\"fields \"/>\n"
"    </signal>\n"
"    <signal name=\"userInputCanceled\"/>\n"
"    <signal name=\"errorReported\">\n"
"      <arg type=\"s\" name=\"error\"/>\n"
"    </signal>\n"
"    <signal name=\"requestBrowser\">\n"
"      <arg type=\"s\" name=\"url\"/>\n"
"    </signal>\n"
"    <signal name=\"connectionRequest\"/>\n"
"  </interface>\n"
        "")
public:
    ConnAdaptor(QObject *parent);
    virtual ~ConnAdaptor();

public: // PROPERTIES
public Q_SLOTS: // METHODS
    void sendConnectReply(const QString &in0, int in1);
    void sendUserReply(const QVariantMap &input);
Q_SIGNALS: // SIGNALS
    void connectionRequest();
    void errorReported(const QString &error);
    void requestBrowser(const QString &url);
    void userInputCanceled();
    void userInputRequested(const QString &service, const QVariantMap &fields);
};

#endif

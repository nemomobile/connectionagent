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

#include "connadaptor.h"
#include <QtCore/QMetaObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

/*
 * Implementation of adaptor class ConnAdaptor
 */

ConnAdaptor::ConnAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    setAutoRelaySignals(true);
}

ConnAdaptor::~ConnAdaptor()
{
}

void ConnAdaptor::sendConnectReply(const QString &in0, int in1)
{
    // handle method call com.jolla.Connectiond.sendConnectReply
    QMetaObject::invokeMethod(parent(), "sendConnectReply", Q_ARG(QString, in0), Q_ARG(int, in1));
}

void ConnAdaptor::sendUserReply(const QVariantMap &input)
{
    // handle method call com.jolla.Connectiond.sendUserReply
    QMetaObject::invokeMethod(parent(), "sendUserReply", Q_ARG(QVariantMap, input));
}


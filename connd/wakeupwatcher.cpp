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

#include <QDBusPendingReply>
#include <QDebug>

#include "wakeupwatcher.h"

WakeupWatcher::WakeupWatcher(QObject *parent) :
    QObject(parent),
    currentPowerSave(false)
{
    mceInterface = new QDBusInterface(MCE_SERVICE,
                                      MCE_SIGNAL_PATH,
                                      MCE_SIGNAL_INTERFACE,
                                      QDBusConnection::systemBus(),
                                      parent);

    mceInterface->connection().connect(MCE_SERVICE,
                                       MCE_SIGNAL_PATH,
                                       MCE_SIGNAL_INTERFACE,
                                       MCE_PSM_STATE_IND,
                                       this,
                                       SLOT(mceSleepStateChanged(bool)));

    mceInterface->connection().connect(MCE_SERVICE,
                                       MCE_SIGNAL_PATH,
                                       MCE_SIGNAL_INTERFACE,
                                       MCE_DISPLAY_IND,
                                       this,
                                       SLOT(mceDisplayStateChanged(const QString)));

    QDBusPendingReply<QString> displayStateReply = QDBusConnection::systemBus().call(
                QDBusMessage::createMethodCall(MCE_SERVICE,
                                               MCE_REQUEST_PATH,
                                               MCE_REQUEST_INTERFACE,
                                               MCE_DISPLAY_STATUS_GET));
    displayStateReply.waitForFinished();
    if (displayStateReply.isValid()) {
        currentDisplayState = displayStateReply.value();
    }
}

void WakeupWatcher::mceDisplayStateChanged(const QString &state)
{
    if (state != currentDisplayState) {
        currentDisplayState = state;
        emit displayStateChanged(currentDisplayState);
    }
}

void WakeupWatcher::mceSleepStateChanged(bool mode)
{
    if (mode != currentPowerSave) {
        currentPowerSave = mode;
        emit sleepStateChanged(currentPowerSave);
    }
}

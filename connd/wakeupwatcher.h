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

#ifndef WAKEUPWATCHER_H
#define WAKEUPWATCHER_H

#include <QObject>
#include <QDBusInterface>

class WakeupWatcher : public QObject
{
    Q_OBJECT
public:
    explicit WakeupWatcher(QObject *parent = 0);

signals:
    void displayStateChanged(const QString&);
    void sleepStateChanged(bool);

private slots:
    void mceDisplayStateChanged(const QString &state);
    void mceSleepStateChanged(bool mode);

private:
    QDBusInterface *mceInterface;
    QString currentDisplayState;
    bool currentPowerSave;
};

#endif // WAKEUPWATCHER_H

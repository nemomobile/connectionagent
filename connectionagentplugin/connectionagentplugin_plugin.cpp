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

#include "connectionagentplugin_plugin.h"
#include "connectionagentplugin.h"

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

void ConnectionagentpluginPlugin::registerTypes(const char *uri)
{
    // @uri com.jolla.connection
    qmlRegisterType<ConnectionAgentPlugin>(uri, 1, 0, "ConnectionAgent");
}

#if QT_VERSION < 0x050000
Q_EXPORT_PLUGIN2(Connectionagentplugin, ConnectionagentpluginPlugin)
#endif

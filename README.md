connectionagent
===============

Connectionagent is a daemon and declarative plugin for accessing connman's UserAgent. It allows multiple access to UserAgent.

The daemon and QtDeclarative plugin are licensed un ther LGPL.


This class is for accessing connman's UserAgent from multiple sources.
This is because currently, there can only be one UserAgent per system.

It also makes use of a patch to connman, that allows the UserAgent
to get signaled when a connection is needed. This is the real reason
this deamon is needed. An InputRequest is short lived, and thus, may
not clash with other apps that need to use UserAgent.

When you are trying to intercept a connection request, you need a long
living process to wait until such time. This will immediately clash if
a wlan needs user Input signal from connman, and the configure will never
get the proper signal.

This qml type can be used as such:

import com.jolla.connection 1.0

    ConnectionAgent {
       id: userAgent
        onUserInputRequested: {
            console.log("        onUserInputRequested:")
        }

       onConnectionRequest: {
          console.log("onConnectionRequest ")
            sendSuppress()
        }
        onErrorReported: {
            console.log("Got error from connman: " + error);
       }
   }
 
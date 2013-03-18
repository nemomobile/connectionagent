import QtQuick 1.1
import Sailfish.Silica 1.0
import jolla.connection 1.0

Rectangle {
    width: 100
    height: 62
Text {

anchors.fill: parent.fill
}
ConnectionAgent: {

    onConnectionRequest {
        console.log("connection request")
    }
}

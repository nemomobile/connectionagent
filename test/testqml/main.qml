import QtQuick 1.1
import Sailfish.Silica 1.0
//import "pages"
import com.jolla.connection 1.0

ApplicationWindow
{
    //   initialPage: FirstPage { }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    Label {
        id: label
        anchors.fill: parent
    }

    ConnectionAgent {
        id: connagent

        onConnectionRequest: {
            console.log("connection request")
            label.text = "connection request"
        }
        onUserInputRequested: {
            label.text = "user input request"
            console.log("userInput")
        }
    }
}



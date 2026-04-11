import QtQuick

Rectangle {
    id: root
    color: "#000000"

    property bool popupOpen: false

    Clock {
        anchors.centerIn: parent
    }

    Item {
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        width: trackRow.width
        height: parent.height
        visible: mpris.available

        Row {
            id: trackRow
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            Text {
                text: mpris.title ? (mpris.artist ? mpris.artist + " — " + mpris.title : mpris.title) : ""
                color: "#b0ffffff"
                font.pixelSize: 11
                font.weight: Font.Medium
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideRight
                width: Math.min(implicitWidth, 180)
            }

            Text {
                text: mpris.playing ? "♫" : "♪"
                color: "#80ffffff"
                font.pixelSize: 14
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                root.popupOpen = !root.popupOpen
                panel.setPopupOpen(root.popupOpen)
            }
        }
    }
}

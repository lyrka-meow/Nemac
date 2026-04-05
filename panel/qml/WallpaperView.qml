import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    property string statusMsg: ""

    Connections {
        target: wallpapers
        function onStatusText(text) { statusMsg = text }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        Text {
            text: "\uD83D\uDDBC  \u041E\u0431\u043E\u0438"
            color: "#e0e0e0"
            font.pixelSize: 16
            font.bold: true
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 140
            radius: 10
            color: Qt.rgba(1, 1, 1, 0.04)

            Image {
                anchors.fill: parent
                anchors.margins: 4
                source: wallpapers.selectedPath.length > 0
                        ? "file://" + wallpapers.selectedPath : ""
                fillMode: Image.PreserveAspectFit
                visible: wallpapers.selectedPath.length > 0
            }
            Text {
                anchors.centerIn: parent
                text: "\u0412\u044B\u0431\u0435\u0440\u0438\u0442\u0435 \u043E\u0431\u043E\u0438"
                color: "#555"
                font.pixelSize: 13
                visible: wallpapers.selectedPath.length === 0
            }
        }

        ListView {
            id: wpList
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: wallpapers
            clip: true
            spacing: 2

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                contentItem: Rectangle {
                    implicitWidth: 5; radius: 2
                    color: Qt.rgba(1, 1, 1, 0.15)
                }
            }

            delegate: Rectangle {
                width: wpList.width
                height: 36
                radius: 8
                color: wpMa.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10

                    Text {
                        text: "\uD83D\uDDBC"
                        font.pixelSize: 14
                    }
                    Text {
                        text: model.name
                        color: "#e0e0e0"
                        font.pixelSize: 13
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                }

                MouseArea {
                    id: wpMa
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: wallpapers.select(index)
                }
            }
        }

        RowLayout {
            spacing: 8

            Button {
                text: "+ \u0414\u043E\u0431\u0430\u0432\u0438\u0442\u044C"
                font.pixelSize: 12
                flat: true
                onClicked: wallpapers.addDialog()
                background: Rectangle {
                    radius: 8
                    color: Qt.rgba(1, 1, 1, 0.08)
                }
                contentItem: Text {
                    text: parent.text; color: "#e0e0e0"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                }
            }
            Item { Layout.fillWidth: true }
            Button {
                text: "\u0423\u0441\u0442\u0430\u043D\u043E\u0432\u0438\u0442\u044C"
                font.pixelSize: 12
                enabled: wallpapers.selectedPath.length > 0
                onClicked: wallpapers.apply()
                background: Rectangle {
                    radius: 8
                    color: Qt.rgba(0.53, 0.67, 1, 0.2)
                    opacity: parent.enabled ? 1.0 : 0.4
                }
                contentItem: Text {
                    text: parent.text; color: "#e0e0e0"
                    font.pixelSize: 12; font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    opacity: parent.enabled ? 1.0 : 0.4
                }
            }
        }

        Text {
            text: statusMsg.length > 0 ? statusMsg : wallpapers.count + " \u043E\u0431\u043E\u0435\u0432"
            color: "#666"
            font.pixelSize: 11
        }
    }
}

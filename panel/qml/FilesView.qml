import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            spacing: 4

            RoundButton {
                text: "\u2B06"
                implicitWidth: 32; implicitHeight: 32
                flat: true
                onClicked: fileModel.cdUp()
                ToolTip.text: "\u041D\u0430\u0437\u0430\u0434"
                ToolTip.visible: hovered
            }
            RoundButton {
                text: "\uD83C\uDFE0"
                implicitWidth: 32; implicitHeight: 32
                flat: true
                onClicked: fileModel.goHome()
                ToolTip.text: "\u0414\u043E\u043C\u043E\u0439"
                ToolTip.visible: hovered
            }
            TextField {
                id: pathField
                Layout.fillWidth: true
                text: fileModel.path
                placeholderText: "\u041F\u0443\u0442\u044C..."
                color: "#e0e0e0"
                font.pixelSize: 12
                background: Rectangle {
                    radius: 8
                    color: Qt.rgba(1, 1, 1, 0.06)
                    border.color: pathField.activeFocus ? Qt.rgba(0.53, 0.67, 1, 0.5) : Qt.rgba(1, 1, 1, 0.1)
                }
                onAccepted: fileModel.cd(text)
            }
        }

        ListView {
            id: fileList
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: fileModel
            clip: true
            spacing: 2

            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
                contentItem: Rectangle {
                    implicitWidth: 5
                    radius: 2
                    color: Qt.rgba(1, 1, 1, 0.15)
                }
            }

            delegate: Rectangle {
                width: fileList.width
                height: 36
                radius: 8
                color: fileMa.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    Text {
                        text: model.icon
                        font.pixelSize: 14
                    }
                    Text {
                        text: model.name
                        color: model.isDir ? "#8af" : "#e0e0e0"
                        font.pixelSize: 13
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    Text {
                        text: model.sizeStr
                        color: "#666"
                        font.pixelSize: 11
                        visible: !model.isDir
                    }
                }

                MouseArea {
                    id: fileMa
                    anchors.fill: parent
                    hoverEnabled: true
                    onDoubleClicked: fileModel.open(index)
                }
            }
        }

        Text {
            text: fileModel.dirs + " \u043F\u0430\u043F\u043E\u043A, " + fileModel.files + " \u0444\u0430\u0439\u043B\u043E\u0432"
            color: "#666"
            font.pixelSize: 11
        }
    }
}

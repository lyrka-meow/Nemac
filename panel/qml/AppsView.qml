import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    property int subTab: 0

    ListModel { id: searchResults }

    Connections {
        target: packages
        function onResultsReady(list) {
            searchResults.clear()
            for (var i = 0; i < list.length; i++)
                searchResults.append(list[i])
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            spacing: 4
            Repeater {
                model: [
                    "\u2B50 \u0420\u0435\u043A\u043E\u043C\u0435\u043D\u0434\u0443\u0435\u043C\u044B\u0435",
                    "\uD83D\uDCE6 Pacman",
                    "\uD83D\uDCE6 AUR"
                ]
                delegate: Button {
                    text: modelData
                    flat: true
                    font.pixelSize: 12
                    checkable: true
                    checked: subTab === index
                    onClicked: subTab = index
                    background: Rectangle {
                        radius: 6
                        color: checked ? Qt.rgba(0.53, 0.67, 1, 0.18) : Qt.rgba(1, 1, 1, 0.06)
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.checked ? "#fff" : "#aaa"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }

        RowLayout {
            visible: subTab === 1 || subTab === 2
            spacing: 4

            TextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: subTab === 1 ? "\u041F\u043E\u0438\u0441\u043A \u0432 pacman..." : "\u041F\u043E\u0438\u0441\u043A \u0432 AUR..."
                color: "#e0e0e0"
                font.pixelSize: 12
                background: Rectangle {
                    radius: 8
                    color: Qt.rgba(1, 1, 1, 0.06)
                    border.color: searchField.activeFocus ? Qt.rgba(0.53, 0.67, 1, 0.5) : Qt.rgba(1, 1, 1, 0.1)
                }
                onAccepted: doSearch()
            }
            RoundButton {
                text: "\uD83D\uDD0D"
                implicitWidth: 36; implicitHeight: 36
                flat: true
                onClicked: doSearch()
            }
        }

        ListView {
            id: recList
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: subTab === 0
            model: packages.recommended
            clip: true
            spacing: 4

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                width: recList.width
                height: 56
                radius: 8
                color: recMa.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Text {
                            text: modelData.name
                            color: "#e0e0e0"
                            font.pixelSize: 14
                            font.bold: true
                        }
                        Text {
                            text: modelData.desc
                            color: "#888"
                            font.pixelSize: 11
                        }
                    }
                    Button {
                        text: "\u0423\u0441\u0442\u0430\u043D\u043E\u0432\u0438\u0442\u044C"
                        font.pixelSize: 12
                        implicitWidth: 100
                        onClicked: packages.install(modelData.pkg, modelData.aur)
                        background: Rectangle {
                            radius: 8
                            color: Qt.rgba(0.53, 0.67, 1, 0.2)
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "#e0e0e0"
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }

                MouseArea {
                    id: recMa
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }
        }

        ListView {
            id: resultList
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: subTab !== 0
            model: searchResults
            clip: true
            spacing: 4

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                width: resultList.width
                height: 56
                radius: 8
                color: resMa.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        Text {
                            text: model.name
                            color: "#e0e0e0"
                            font.pixelSize: 14
                            font.bold: true
                        }
                        Text {
                            text: model.desc
                            color: "#888"
                            font.pixelSize: 11
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }
                    Button {
                        text: "\u0423\u0441\u0442."
                        font.pixelSize: 12
                        implicitWidth: 60
                        onClicked: packages.install(model.name, model.aur)
                        background: Rectangle {
                            radius: 8
                            color: Qt.rgba(1, 1, 1, 0.08)
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "#e0e0e0"
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }

                MouseArea {
                    id: resMa
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }
        }

        Text {
            text: packages.status
            color: "#666"
            font.pixelSize: 11
        }
    }

    function doSearch() {
        if (subTab === 1)
            packages.searchPacman(searchField.text)
        else if (subTab === 2)
            packages.searchAur(searchField.text)
    }
}

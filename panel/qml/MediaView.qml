import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            spacing: 14
            visible: mpris.active

            Rectangle {
                width: 120; height: 120
                radius: 12
                color: Qt.rgba(1, 1, 1, 0.04)

                Image {
                    anchors.fill: parent
                    anchors.margins: 2
                    source: mpris.cover.length > 0 ? "file://" + mpris.cover : ""
                    fillMode: Image.PreserveAspectCrop
                    visible: mpris.cover.length > 0
                    layer.enabled: true
                    layer.effect: Item {}
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Item { Layout.fillHeight: true }
                Text {
                    text: mpris.track
                    color: "#fff"
                    font.pixelSize: 16
                    font.bold: true
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                    maximumLineCount: 2
                }
                Text {
                    text: mpris.artist
                    color: "#aaa"
                    font.pixelSize: 13
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                Item { Layout.fillHeight: true }
            }
        }

        RowLayout {
            visible: mpris.active
            spacing: 4

            Text {
                text: formatUs(mpris.posUs)
                color: "#888"
                font.pixelSize: 11
            }
            Slider {
                id: seekSlider
                Layout.fillWidth: true
                from: 0; to: 1000
                value: mpris.durUs > 0 ? (mpris.posUs / mpris.durUs * 1000) : 0
                onPressedChanged: {
                    if (!pressed)
                        mpris.seek(value / 1000.0)
                }

                background: Rectangle {
                    x: seekSlider.leftPadding
                    y: seekSlider.topPadding + seekSlider.availableHeight / 2 - 2
                    width: seekSlider.availableWidth; height: 4
                    radius: 2
                    color: Qt.rgba(1, 1, 1, 0.1)
                    Rectangle {
                        width: seekSlider.visualPosition * parent.width
                        height: parent.height
                        radius: 2
                        color: "#88aaff"
                    }
                }
                handle: Rectangle {
                    x: seekSlider.leftPadding + seekSlider.visualPosition * (seekSlider.availableWidth - width)
                    y: seekSlider.topPadding + seekSlider.availableHeight / 2 - 6
                    width: 12; height: 12
                    radius: 6
                    color: "#88aaff"
                }
            }
            Text {
                text: formatUs(mpris.durUs)
                color: "#888"
                font.pixelSize: 11
            }
        }

        RowLayout {
            visible: mpris.active
            Layout.alignment: Qt.AlignHCenter
            spacing: 12

            RoundButton {
                text: "\u23EE"
                font.pixelSize: 18
                implicitWidth: 40; implicitHeight: 40
                flat: true
                onClicked: mpris.prev()
            }
            RoundButton {
                text: mpris.playing ? "\u23F8" : "\u25B6"
                font.pixelSize: 22
                implicitWidth: 52; implicitHeight: 52
                flat: true
                background: Rectangle {
                    radius: 26
                    color: Qt.rgba(0.53, 0.67, 1.0, 0.2)
                }
                onClicked: mpris.playPause()
            }
            RoundButton {
                text: "\u23ED"
                font.pixelSize: 18
                implicitWidth: 40; implicitHeight: 40
                flat: true
                onClicked: mpris.next()
            }
        }

        Text {
            visible: !mpris.active
            text: "\u041D\u0438\u0447\u0435\u0433\u043E \u043D\u0435 \u0438\u0433\u0440\u0430\u0435\u0442"
            color: "#666"
            font.pixelSize: 15
            Layout.alignment: Qt.AlignCenter
            Layout.topMargin: 40
        }

        Item { Layout.fillHeight: true }
    }

    function formatUs(us) {
        if (us < 0) us = 0
        var s = Math.floor(us / 1000000)
        var m = Math.floor(s / 60)
        var sec = s % 60
        return m + ":" + (sec < 10 ? "0" : "") + sec
    }
}

import QtQuick

Rectangle {
    id: popup
    color: "#000000"

    Row {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 14

        Item {
            id: artContainer
            width: 120
            height: 120
            anchors.verticalCenter: parent.verticalCenter

            Rectangle {
                anchors.centerIn: parent
                width: 112; height: 112
                radius: 56
                color: "#555555"
            }

            Item {
                anchors.centerIn: parent
                width: 112; height: 112
                clip: true

                Image {
                    id: coverImg
                    anchors.centerIn: parent
                    width: parent.width; height: parent.height
                    source: mpris.artUrl || ""
                    fillMode: Image.PreserveAspectCrop
                    visible: mpris.artUrl !== ""

                    RotationAnimation on rotation {
                        running: mpris.playing
                        from: coverImg.rotation
                        to: coverImg.rotation + 360
                        duration: 20000
                        loops: Animation.Infinite
                    }
                }

                Canvas {
                    anchors.fill: parent
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.fillStyle = "#000000"
                        ctx.fillRect(0, 0, width, height)
                        ctx.globalCompositeOperation = "destination-out"
                        ctx.beginPath()
                        ctx.arc(width / 2, height / 2, width / 2, 0, Math.PI * 2)
                        ctx.fill()
                        ctx.globalCompositeOperation = "source-over"
                        ctx.beginPath()
                        ctx.arc(width / 2, height / 2, 11, 0, Math.PI * 2)
                        ctx.fillStyle = "#000000"
                        ctx.fill()
                        ctx.beginPath()
                        ctx.arc(width / 2, height / 2, 8, 0, Math.PI * 2)
                        ctx.strokeStyle = "rgba(255,255,255,0.35)"
                        ctx.lineWidth = 1.5
                        ctx.stroke()
                        ctx.beginPath()
                        ctx.arc(width / 2, height / 2, 3, 0, Math.PI * 2)
                        ctx.fillStyle = "rgba(255,255,255,0.25)"
                        ctx.fill()
                        ctx.beginPath()
                        ctx.arc(width / 2, height / 2, 28, 0, Math.PI * 2)
                        ctx.strokeStyle = "rgba(255,255,255,0.12)"
                        ctx.lineWidth = 0.5
                        ctx.stroke()
                    }
                }
            }
        }

        Column {
            width: parent.width - artContainer.width - parent.spacing
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            Text {
                width: parent.width
                text: mpris.title || ""
                color: "#f0ffffff"
                font.pixelSize: 14
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: mpris.artist || ""
                color: "#90ffffff"
                font.pixelSize: 12
                elide: Text.ElideRight
            }

            Item { width: 1; height: 12 }

            Row {
                spacing: 18
                anchors.horizontalCenter: parent.horizontalCenter

                Image {
                    width: 20; height: 20
                    source: "qrc:/icons/prev.svg"
                    opacity: prevArea.containsMouse ? 1.0 : 0.7
                    MouseArea {
                        id: prevArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: mpris.previous()
                    }
                }

                Image {
                    width: 24; height: 24
                    source: mpris.playing ? "qrc:/icons/pause.svg" : "qrc:/icons/play.svg"
                    opacity: playArea.containsMouse ? 1.0 : 0.7
                    MouseArea {
                        id: playArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: mpris.playPause()
                    }
                }

                Image {
                    width: 20; height: 20
                    source: "qrc:/icons/next.svg"
                    opacity: nextArea.containsMouse ? 1.0 : 0.7
                    MouseArea {
                        id: nextArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: mpris.next()
                    }
                }
            }
        }
    }
}

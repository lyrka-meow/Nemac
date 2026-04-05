import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: root
    width: 420; height: 520
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool | Qt.X11BypassWindowManagerHint
    color: "transparent"
    visible: false
    x: bridge.panelX; y: bridge.panelY

    property int currentTab: 0

    Connections {
        target: bridge
        function onPanelOpenChanged() {
            if (bridge.panelOpen) {
                root.opacity = 0
                root.visible = true
                openAnim.start()
            } else {
                closeAnim.start()
            }
        }
    }

    NumberAnimation {
        id: openAnim
        target: root; property: "opacity"
        from: 0; to: 1; duration: 150
        easing.type: Easing.OutQuad
    }
    NumberAnimation {
        id: closeAnim
        target: root; property: "opacity"
        from: 1; to: 0; duration: 120
        easing.type: Easing.InQuad
        onFinished: root.visible = false
    }

    Rectangle {
        anchors.fill: parent
        radius: 16
        color: Qt.rgba(0.07, 0.07, 0.09, 0.92)
        border.color: Qt.rgba(1, 1, 1, 0.07)
        border.width: 1
        clip: true

        RowLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillHeight: true
                Layout.preferredWidth: 52
                color: "transparent"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 4
                    anchors.topMargin: 12
                    spacing: 6

                    Repeater {
                        model: [
                            {icon: "\uD83C\uDFB5", tip: "Медиа"},
                            {icon: "\uD83D\uDCC1", tip: "Файлы"},
                            {icon: "\uD83D\uDCE6", tip: "Приложения"},
                            {icon: "\uD83D\uDDBC", tip: "Обои"},
                            {icon: "\uD83D\uDD50", tip: "Часы"},
                            {icon: "\u25BA",        tip: "Терминал"}
                        ]
                        delegate: Rectangle {
                            Layout.preferredWidth: 44
                            Layout.preferredHeight: 44
                            Layout.alignment: Qt.AlignHCenter
                            radius: 10
                            color: root.currentTab === index
                                   ? Qt.rgba(1, 1, 1, 0.14)
                                   : tabMa.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : "transparent"

                            Text {
                                anchors.centerIn: parent
                                text: modelData.icon
                                font.pixelSize: 20
                            }

                            ToolTip.visible: tabMa.containsMouse
                            ToolTip.text: modelData.tip
                            ToolTip.delay: 500

                            MouseArea {
                                id: tabMa
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: root.currentTab = index
                            }
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: root.currentTab

                MediaView {}
                FilesView {}
                AppsView {}
                WallpaperView {}
                ClockView {}
                TermView {}
            }
        }
    }
}

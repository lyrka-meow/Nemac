import QtQuick

Text {
    color: "#e8ffffff"
    font.pixelSize: 13
    font.weight: Font.Medium
    font.letterSpacing: 0.4

    Timer {
        interval: 30000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: parent.text = Qt.formatTime(new Date(), "HH:mm")
    }
}

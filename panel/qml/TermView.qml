import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            TextArea {
                id: termOutput
                readOnly: true
                text: term.output
                color: "#ddd"
                font.family: "JetBrains Mono, Fira Code, monospace"
                font.pixelSize: 11
                wrapMode: TextEdit.Wrap
                background: Rectangle {
                    radius: 8
                    color: Qt.rgba(0, 0, 0, 0.3)
                    border.color: Qt.rgba(1, 1, 1, 0.06)
                }
                padding: 8

                onTextChanged: {
                    cursorPosition = length
                }
            }
        }

        RowLayout {
            spacing: 6

            Text {
                text: "$"
                color: "#88aaff"
                font.family: "monospace"
                font.pixelSize: 13
                font.bold: true
            }
            TextField {
                id: cmdInput
                Layout.fillWidth: true
                placeholderText: "\u041A\u043E\u043C\u0430\u043D\u0434\u0430..."
                enabled: !term.busy
                color: "#e0e0e0"
                font.family: "JetBrains Mono, Fira Code, monospace"
                font.pixelSize: 11
                background: Rectangle {
                    radius: 6
                    color: Qt.rgba(0, 0, 0, 0.2)
                    border.color: cmdInput.activeFocus ? Qt.rgba(0.53, 0.67, 1, 0.5) : Qt.rgba(1, 1, 1, 0.08)
                }
                onAccepted: {
                    term.exec(text)
                    text = ""
                }
            }
        }
    }
}

import QtQuick
import QtQuick.Layouts

Item {
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 4

        Text {
            id: timeText
            Layout.alignment: Qt.AlignHCenter
            color: "#fff"
            font.pixelSize: 48
            font.bold: true
        }
        Text {
            id: dateText
            Layout.alignment: Qt.AlignHCenter
            color: "#aaa"
            font.pixelSize: 14
        }
    }

    Timer {
        interval: 1000; running: true; repeat: true; triggeredOnStart: true
        onTriggered: {
            var now = new Date()
            var h = now.getHours()
            var m = now.getMinutes()
            var s = now.getSeconds()
            timeText.text = pad(h) + ":" + pad(m) + ":" + pad(s)

            var days = ["\u0412\u043E\u0441\u043A\u0440\u0435\u0441\u0435\u043D\u044C\u0435",
                        "\u041F\u043E\u043D\u0435\u0434\u0435\u043B\u044C\u043D\u0438\u043A",
                        "\u0412\u0442\u043E\u0440\u043D\u0438\u043A",
                        "\u0421\u0440\u0435\u0434\u0430",
                        "\u0427\u0435\u0442\u0432\u0435\u0440\u0433",
                        "\u041F\u044F\u0442\u043D\u0438\u0446\u0430",
                        "\u0421\u0443\u0431\u0431\u043E\u0442\u0430"]
            var months = ["\u044F\u043D\u0432\u0430\u0440\u044F", "\u0444\u0435\u0432\u0440\u0430\u043B\u044F",
                          "\u043C\u0430\u0440\u0442\u0430", "\u0430\u043F\u0440\u0435\u043B\u044F",
                          "\u043C\u0430\u044F", "\u0438\u044E\u043D\u044F",
                          "\u0438\u044E\u043B\u044F", "\u0430\u0432\u0433\u0443\u0441\u0442\u0430",
                          "\u0441\u0435\u043D\u0442\u044F\u0431\u0440\u044F", "\u043E\u043A\u0442\u044F\u0431\u0440\u044F",
                          "\u043D\u043E\u044F\u0431\u0440\u044F", "\u0434\u0435\u043A\u0430\u0431\u0440\u044F"]
            dateText.text = days[now.getDay()] + ", " + now.getDate() + " " +
                            months[now.getMonth()] + " " + now.getFullYear()
        }
    }

    function pad(n) { return n < 10 ? "0" + n : "" + n }
}

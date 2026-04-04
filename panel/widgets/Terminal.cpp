#include "Terminal.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QFont>

Terminal::Terminal(QWidget* parent) : QWidget(parent) {
    _cwd = QDir::homePath();

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(10, 10, 10, 10);
    lay->setSpacing(6);

    _notice = new QLabel(
        "\xD0\xA2\xD0\xB5\xD1\x80\xD0\xBC\xD0\xB8\xD0\xBD\xD0\xB0\xD0\xBB "
        "\xD0\xBE\xD1\x82\xD0\xBA\xD0\xBB\xD1\x8E\xD1\x87\xD1\x91\xD0\xBD. "
        "\xD0\x92\xD0\xBA\xD0\xBB\xD1\x8E\xD1\x87\xD0\xB8\xD1\x82\xD0\xB5 "
        "\xD0\xB2 \xD0\xBD\xD0\xB0\xD1\x81\xD1\x82\xD1\x80\xD0\xBE\xD0\xB9\xD0\xBA\xD0\xB0\xD1\x85.",
        this);
    _notice->setAlignment(Qt::AlignCenter);
    _notice->setStyleSheet("font-size: 14px; color: #666; padding: 60px 20px;");
    lay->addWidget(_notice);

    _output = new QPlainTextEdit(this);
    _output->setReadOnly(true);
    _output->setFont(QFont("JetBrains Mono, Fira Code, monospace", 11));
    _output->setStyleSheet(R"(
        QPlainTextEdit {
            background: rgba(0,0,0,0.3);
            border: 1px solid rgba(255,255,255,0.06);
            border-radius: 8px;
            padding: 8px;
            color: #ddd;
        }
    )");
    _output->hide();
    lay->addWidget(_output, 1);

    auto* inputLay = new QHBoxLayout;
    _prompt = new QLabel("$", this);
    _prompt->setStyleSheet("font-family: monospace; font-size: 13px; color: #88aaff; font-weight: bold;");
    _prompt->hide();
    _input = new QLineEdit(this);
    _input->setFont(QFont("JetBrains Mono, Fira Code, monospace", 11));
    _input->setPlaceholderText("\xD0\x9A\xD0\xBE\xD0\xBC\xD0\xB0\xD0\xBD\xD0\xB4\xD0\xB0...");
    _input->setStyleSheet(R"(
        QLineEdit {
            background: rgba(0,0,0,0.2);
            border: 1px solid rgba(255,255,255,0.08);
            border-radius: 6px;
            padding: 6px 10px;
            font-family: monospace;
            color: #e0e0e0;
        }
    )");
    _input->hide();
    inputLay->addWidget(_prompt);
    inputLay->addWidget(_input, 1);
    lay->addLayout(inputLay);

    _proc = new QProcess(this);
    _proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(_proc, &QProcess::readyRead, this, &Terminal::onOutput);
    connect(_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Terminal::onFinished);
    connect(_input, &QLineEdit::returnPressed, this, &Terminal::onCommand);
}

void Terminal::setEnabled(bool on) {
    _enabled = on;
    _notice->setVisible(!on);
    _output->setVisible(on);
    _input->setVisible(on);
    _prompt->setVisible(on);
    if (on) {
        _output->clear();
        _output->appendPlainText(QString("~ nemac terminal [%1]").arg(_cwd));
        _input->setFocus();
    }
}

void Terminal::onCommand() {
    QString cmd = _input->text().trimmed();
    _input->clear();
    if (cmd.isEmpty()) return;

    _output->appendPlainText(QString("$ %1").arg(cmd));

    if (cmd.startsWith("cd ")) {
        QString dir = cmd.mid(3).trimmed();
        if (dir == "~") dir = QDir::homePath();
        QDir d(dir);
        if (!d.isAbsolute()) d = QDir(_cwd + "/" + dir);
        if (d.exists()) {
            _cwd = d.canonicalPath();
            _output->appendPlainText(QString("[%1]").arg(_cwd));
        } else {
            _output->appendPlainText(
                "\xD0\x9F\xD0\xB0\xD0\xBF\xD0\xBA\xD0\xB0 \xD0\xBD\xD0\xB5 \xD0\xBD\xD0\xB0\xD0\xB9\xD0\xB4\xD0\xB5\xD0\xBD\xD0\xB0: " + dir);
        }
        return;
    }

    _input->setEnabled(false);
    _proc->setWorkingDirectory(_cwd);
    _proc->start("sh", {"-c", cmd});
}

void Terminal::onOutput() {
    QByteArray data = _proc->readAll();
    QString text = QString::fromUtf8(data);
    if (!text.isEmpty()) {
        if (text.endsWith('\n')) text.chop(1);
        _output->appendPlainText(text);
    }
}

void Terminal::onFinished(int code, QProcess::ExitStatus) {
    if (code != 0)
        _output->appendPlainText(QString("[\xD0\xBA\xD0\xBE\xD0\xB4: %1]").arg(code));
    _input->setEnabled(true);
    _input->setFocus();
}

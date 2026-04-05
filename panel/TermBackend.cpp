#include "TermBackend.h"
#include <QDir>

TermBackend::TermBackend(QObject* parent) : QObject(parent) {
    _cwd = QDir::homePath();

    _proc = new QProcess(this);
    _proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(_proc, &QProcess::readyRead, this, [this]() {
        QByteArray data = _proc->readAll();
        QString text = QString::fromUtf8(data);
        if (!text.isEmpty()) {
            if (text.endsWith('\n')) text.chop(1);
            appendLine(text);
        }
    });

    connect(_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
        if (code != 0)
            appendLine(QString("[\xD0\xBA\xD0\xBE\xD0\xB4: %1]").arg(code));
        _busy = false;
        emit busyChanged();
    });

    appendLine(QString("~ nemac terminal [%1]").arg(_cwd));
}

void TermBackend::exec(const QString& cmd) {
    QString trimmed = cmd.trimmed();
    if (trimmed.isEmpty()) return;

    appendLine(QString("$ %1").arg(trimmed));

    if (trimmed.startsWith("cd ")) {
        QString dir = trimmed.mid(3).trimmed();
        if (dir == "~") dir = QDir::homePath();
        QDir d(dir);
        if (!d.isAbsolute()) d = QDir(_cwd + "/" + dir);
        if (d.exists()) {
            _cwd = d.canonicalPath();
            appendLine(QString("[%1]").arg(_cwd));
            emit cwdChanged();
        } else {
            appendLine("\xD0\x9F\xD0\xB0\xD0\xBF\xD0\xBA\xD0\xB0 \xD0\xBD\xD0\xB5 \xD0\xBD\xD0\xB0\xD0\xB9\xD0\xB4\xD0\xB5\xD0\xBD\xD0\xB0: " + dir);
        }
        return;
    }

    _busy = true;
    emit busyChanged();
    _proc->setWorkingDirectory(_cwd);
    _proc->start("sh", {"-c", trimmed});
}

void TermBackend::clear() {
    _output.clear();
    appendLine(QString("~ nemac terminal [%1]").arg(_cwd));
}

void TermBackend::appendLine(const QString& line) {
    if (!_output.isEmpty()) _output += "\n";
    _output += line;
    emit outputChanged();
}

#include "PackageBackend.h"
#include <QVariantMap>

PackageBackend::PackageBackend(QObject* parent) : QObject(parent) {
    _proc = new QProcess(this);
    connect(_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
        setStatus(code == 0
            ? "\xE2\x9C\x85 \xD0\x93\xD0\xBE\xD1\x82\xD0\xBE\xD0\xB2\xD0\xBE"
            : "\xE2\x9D\x8C \xD0\x9E\xD1\x88\xD0\xB8\xD0\xB1\xD0\xBA\xD0\xB0");
    });

    _recommended = {
        QVariantMap{{"name", "Vesktop"},     {"desc", "Discord \xD0\xBA\xD0\xBB\xD0\xB8\xD0\xB5\xD0\xBD\xD1\x82"}, {"pkg", "vesktop-bin"}, {"aur", true}},
        QVariantMap{{"name", "Zen Browser"}, {"desc", "\xD0\x91\xD1\x80\xD0\xB0\xD1\x83\xD0\xB7\xD0\xB5\xD1\x80"}, {"pkg", "zen-browser-bin"}, {"aur", true}},
        QVariantMap{{"name", "Ayugram"},     {"desc", "Telegram \xD0\xBA\xD0\xBB\xD0\xB8\xD0\xB5\xD0\xBD\xD1\x82"}, {"pkg", "ayugram"}, {"aur", false}},
        QVariantMap{{"name", "Firefox"},     {"desc", "\xD0\x91\xD1\x80\xD0\xB0\xD1\x83\xD0\xB7\xD0\xB5\xD1\x80"}, {"pkg", "firefox"}, {"aur", false}},
        QVariantMap{{"name", "VLC"},         {"desc", "\xD0\x9C\xD0\xB5\xD0\xB4\xD0\xB8\xD0\xB0\xD0\xBF\xD0\xBB\xD0\xB5\xD0\xB5\xD1\x80"}, {"pkg", "vlc"}, {"aur", false}},
        QVariantMap{{"name", "OBS Studio"},  {"desc", "\xD0\x97\xD0\xB0\xD0\xBF\xD0\xB8\xD1\x81\xD1\x8C \xD1\x8D\xD0\xBA\xD1\x80\xD0\xB0\xD0\xBD\xD0\xB0"}, {"pkg", "obs-studio"}, {"aur", false}},
        QVariantMap{{"name", "GIMP"},        {"desc", "\xD0\x93\xD1\x80\xD0\xB0\xD1\x84\xD0\xB8\xD1\x87\xD0\xB5\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9 \xD1\x80\xD0\xB5\xD0\xB4\xD0\xB0\xD0\xBA\xD1\x82\xD0\xBE\xD1\x80"}, {"pkg", "gimp"}, {"aur", false}},
        QVariantMap{{"name", "VSCode"},      {"desc", "\xD0\xA0\xD0\xB5\xD0\xB4\xD0\xB0\xD0\xBA\xD1\x82\xD0\xBE\xD1\x80 \xD0\xBA\xD0\xBE\xD0\xB4\xD0\xB0"}, {"pkg", "visual-studio-code-bin"}, {"aur", true}},
        QVariantMap{{"name", "Steam"},       {"desc", "\xD0\x98\xD0\xB3\xD1\x80\xD1\x8B"}, {"pkg", "steam"}, {"aur", false}},
        QVariantMap{{"name", "htop"},        {"desc", "\xD0\x9C\xD0\xBE\xD0\xBD\xD0\xB8\xD1\x82\xD0\xBE\xD1\x80 \xD0\xBF\xD1\x80\xD0\xBE\xD1\x86\xD0\xB5\xD1\x81\xD1\x81\xD0\xBE\xD0\xB2"}, {"pkg", "htop"}, {"aur", false}},
    };
}

void PackageBackend::setStatus(const QString& s) {
    _status = s;
    emit statusChanged();
}

QVariantList PackageBackend::parseResults(const QString& output, bool aur) {
    QVariantList out;
    auto lines = output.split('\n');
    for (int i = 0; i + 1 < lines.size(); i += 2) {
        QString header = lines[i].trimmed();
        QString desc   = lines[i + 1].trimmed();
        if (header.isEmpty()) continue;

        auto parts = header.split(' ');
        QString fullName = parts.value(0);
        QString name = fullName.contains('/') ? fullName.split('/').last() : fullName;

        out.append(QVariantMap{{"name", name}, {"desc", desc}, {"aur", aur}});
    }
    return out;
}

void PackageBackend::searchPacman(const QString& query) {
    if (query.trimmed().isEmpty()) return;
    setStatus("\xD0\x9F\xD0\xBE\xD0\xB8\xD1\x81\xD0\xBA...");

    QProcess proc;
    proc.start("pacman", {"-Ss", query.trimmed()});
    proc.waitForFinished(5000);
    auto list = parseResults(proc.readAllStandardOutput(), false);
    emit resultsReady(list);
    setStatus(QString("\xD0\x9D\xD0\xB0\xD0\xB9\xD0\xB4\xD0\xB5\xD0\xBD\xD0\xBE: %1").arg(list.size()));
}

void PackageBackend::searchAur(const QString& query) {
    if (query.trimmed().isEmpty()) return;
    setStatus("\xD0\x9F\xD0\xBE\xD0\xB8\xD1\x81\xD0\xBA \xD0\xB2 AUR...");

    QString helper;
    QProcess test;
    test.start("which", {"yay"});
    test.waitForFinished(1000);
    if (test.exitCode() == 0) helper = "yay";
    else {
        test.start("which", {"paru"});
        test.waitForFinished(1000);
        if (test.exitCode() == 0) helper = "paru";
    }

    if (helper.isEmpty()) {
        setStatus("\xD0\x9D\xD0\xB5\xD1\x82 AUR-\xD1\x85\xD0\xB5\xD0\xBB\xD0\xBF\xD0\xB5\xD1\x80\xD0\xB0 (yay/paru)");
        return;
    }

    QProcess proc;
    proc.start(helper, {"-Ss", query.trimmed()});
    proc.waitForFinished(10000);
    auto list = parseResults(proc.readAllStandardOutput(), true);
    emit resultsReady(list);
    setStatus(QString("\xD0\x9D\xD0\xB0\xD0\xB9\xD0\xB4\xD0\xB5\xD0\xBD\xD0\xBE: %1").arg(list.size()));
}

void PackageBackend::install(const QString& name, bool aur) {
    if (aur) {
        QProcess test;
        test.start("which", {"yay"});
        test.waitForFinished(1000);
        QString helper = (test.exitCode() == 0) ? "yay" : "paru";
        setStatus("\xD0\xA3\xD1\x81\xD1\x82\xD0\xB0\xD0\xBD\xD0\xBE\xD0\xB2\xD0\xBA\xD0\xB0 " + name + "...");
        _proc->start("sh", {"-c",
            QString("pkexec %1 -S --noconfirm %2").arg(helper, name)});
    } else {
        setStatus("\xD0\xA3\xD1\x81\xD1\x82\xD0\xB0\xD0\xBD\xD0\xBE\xD0\xB2\xD0\xBA\xD0\xB0 " + name + "...");
        _proc->start("sh", {"-c",
            QString("pkexec pacman -S --noconfirm %1").arg(name)});
    }
}

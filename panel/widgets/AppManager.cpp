#include "AppManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFont>

AppManager::AppManager(QWidget* parent) : QWidget(parent) {
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(10, 10, 10, 10);
    lay->setSpacing(8);

    _tabs = new QTabWidget(this);
    lay->addWidget(_tabs, 1);

    /* ======== Рекомендуемые ======== */
    auto* recWidget = new QWidget(this);
    auto* recLay = new QVBoxLayout(recWidget);
    recLay->setContentsMargins(4, 8, 4, 4);
    _recList = new QListWidget(recWidget);
    recLay->addWidget(_recList);
    _tabs->addTab(recWidget, "\xE2\xAD\x90 \xD0\xA0\xD0\xB5\xD0\xBA\xD0\xBE\xD0\xBC\xD0\xB5\xD0\xBD\xD0\xB4\xD1\x83\xD0\xB5\xD0\xBC\xD1\x8B\xD0\xB5");

    /* ======== Pacman ======== */
    auto* pacWidget = new QWidget(this);
    auto* pacLay = new QVBoxLayout(pacWidget);
    pacLay->setContentsMargins(4, 8, 4, 4);
    auto* pacTopLay = new QHBoxLayout;
    _pacSearch = new QLineEdit(pacWidget);
    _pacSearch->setPlaceholderText("\xD0\x9F\xD0\xBE\xD0\xB8\xD1\x81\xD0\xBA \xD0\xB2 pacman...");
    auto* pacBtn = new QPushButton("\xF0\x9F\x94\x8D", pacWidget);
    pacBtn->setFixedWidth(36);
    pacTopLay->addWidget(_pacSearch, 1);
    pacTopLay->addWidget(pacBtn);
    pacLay->addLayout(pacTopLay);
    _pacList = new QListWidget(pacWidget);
    pacLay->addWidget(_pacList, 1);
    _tabs->addTab(pacWidget, "\xF0\x9F\x93\xA6 Pacman");

    connect(pacBtn, &QPushButton::clicked, this, &AppManager::searchPacman);
    connect(_pacSearch, &QLineEdit::returnPressed, this, &AppManager::searchPacman);

    /* ======== AUR ======== */
    auto* aurWidget = new QWidget(this);
    auto* aurLay = new QVBoxLayout(aurWidget);
    aurLay->setContentsMargins(4, 8, 4, 4);
    auto* aurTopLay = new QHBoxLayout;
    _aurSearch = new QLineEdit(aurWidget);
    _aurSearch->setPlaceholderText("\xD0\x9F\xD0\xBE\xD0\xB8\xD1\x81\xD0\xBA \xD0\xB2 AUR...");
    auto* aurBtn = new QPushButton("\xF0\x9F\x94\x8D", aurWidget);
    aurBtn->setFixedWidth(36);
    aurTopLay->addWidget(_aurSearch, 1);
    aurTopLay->addWidget(aurBtn);
    aurLay->addLayout(aurTopLay);
    _aurList = new QListWidget(aurWidget);
    aurLay->addWidget(_aurList, 1);
    _tabs->addTab(aurWidget, "\xF0\x9F\x93\xA6 AUR");

    connect(aurBtn, &QPushButton::clicked, this, &AppManager::searchAur);
    connect(_aurSearch, &QLineEdit::returnPressed, this, &AppManager::searchAur);

    _status = new QLabel(this);
    _status->setStyleSheet("font-size: 11px; color: #666;");
    lay->addWidget(_status);

    _proc = new QProcess(this);
    connect(_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
        _status->setText(code == 0
            ? "\xE2\x9C\x85 \xD0\x93\xD0\xBE\xD1\x82\xD0\xBE\xD0\xB2\xD0\xBE"
            : "\xE2\x9D\x8C \xD0\x9E\xD1\x88\xD0\xB8\xD0\xB1\xD0\xBA\xD0\xB0");
    });

    populateRecommended();
}

void AppManager::populateRecommended() {
    struct App { QString name; QString desc; QString pkg; bool aur; };
    App apps[] = {
        {"Vesktop",     "Discord \xD0\xBA\xD0\xBB\xD0\xB8\xD0\xB5\xD0\xBD\xD1\x82", "vesktop-bin", true},
        {"Zen Browser", "\xD0\x91\xD1\x80\xD0\xB0\xD1\x83\xD0\xB7\xD0\xB5\xD1\x80", "zen-browser-bin", true},
        {"Ayugram",     "Telegram \xD0\xBA\xD0\xBB\xD0\xB8\xD0\xB5\xD0\xBD\xD1\x82 (flatpak)", "ayugram", false},
        {"Firefox",     "\xD0\x91\xD1\x80\xD0\xB0\xD1\x83\xD0\xB7\xD0\xB5\xD1\x80", "firefox", false},
        {"VLC",         "\xD0\x9C\xD0\xB5\xD0\xB4\xD0\xB8\xD0\xB0\xD0\xBF\xD0\xBB\xD0\xB5\xD0\xB5\xD1\x80", "vlc", false},
        {"OBS Studio",  "\xD0\x97\xD0\xB0\xD0\xBF\xD0\xB8\xD1\x81\xD1\x8C \xD1\x8D\xD0\xBA\xD1\x80\xD0\xB0\xD0\xBD\xD0\xB0", "obs-studio", false},
        {"GIMP",        "\xD0\x93\xD1\x80\xD0\xB0\xD1\x84\xD0\xB8\xD1\x87\xD0\xB5\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9 \xD1\x80\xD0\xB5\xD0\xB4\xD0\xB0\xD0\xBA\xD1\x82\xD0\xBE\xD1\x80", "gimp", false},
        {"VSCode",      "\xD0\xA0\xD0\xB5\xD0\xB4\xD0\xB0\xD0\xBA\xD1\x82\xD0\xBE\xD1\x80 \xD0\xBA\xD0\xBE\xD0\xB4\xD0\xB0", "visual-studio-code-bin", true},
        {"Steam",       "\xD0\x98\xD0\xB3\xD1\x80\xD1\x8B", "steam", false},
        {"htop",        "\xD0\x9C\xD0\xBE\xD0\xBD\xD0\xB8\xD1\x82\xD0\xBE\xD1\x80 \xD0\xBF\xD1\x80\xD0\xBE\xD1\x86\xD0\xB5\xD1\x81\xD1\x81\xD0\xBE\xD0\xB2", "htop", false},
    };

    for (auto& a : apps) {
        auto* widget = new QWidget(_recList);
        auto* hlay = new QHBoxLayout(widget);
        hlay->setContentsMargins(8, 6, 8, 6);

        auto* info = new QVBoxLayout;
        auto* nameLabel = new QLabel(a.name, widget);
        nameLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
        auto* descLabel = new QLabel(a.desc, widget);
        descLabel->setStyleSheet("font-size: 11px; color: #888;");
        info->addWidget(nameLabel);
        info->addWidget(descLabel);
        hlay->addLayout(info, 1);

        auto* installBtn = new QPushButton("\xD0\xA3\xD1\x81\xD1\x82\xD0\xB0\xD0\xBD\xD0\xBE\xD0\xB2\xD0\xB8\xD1\x82\xD1\x8C", widget);
        installBtn->setFixedWidth(100);
        installBtn->setStyleSheet("background: rgba(136,170,255,0.2); font-size: 12px;");
        connect(installBtn, &QPushButton::clicked, this, [this, a]() {
            installPackage(a.pkg, a.aur);
        });
        hlay->addWidget(installBtn);

        auto* item = new QListWidgetItem();
        item->setSizeHint(widget->sizeHint());
        _recList->addItem(item);
        _recList->setItemWidget(item, widget);
    }
}

void AppManager::searchPacman() {
    QString query = _pacSearch->text().trimmed();
    if (query.isEmpty()) return;

    _pacList->clear();
    _status->setText("\xD0\x9F\xD0\xBE\xD0\xB8\xD1\x81\xD0\xBA...");

    QProcess proc;
    proc.start("pacman", {"-Ss", query});
    proc.waitForFinished(5000);
    QString out = proc.readAllStandardOutput();

    auto lines = out.split('\n');
    for (int i = 0; i + 1 < lines.size(); i += 2) {
        QString header = lines[i].trimmed();
        QString desc   = lines[i + 1].trimmed();
        if (header.isEmpty()) continue;

        auto parts = header.split(' ');
        QString fullName = parts.value(0);
        QString name = fullName.contains('/') ? fullName.split('/').last() : fullName;

        auto* widget = new QWidget(_pacList);
        auto* hlay = new QHBoxLayout(widget);
        hlay->setContentsMargins(8, 4, 8, 4);

        auto* info = new QVBoxLayout;
        auto* nl = new QLabel(name, widget);
        nl->setStyleSheet("font-weight: bold;");
        auto* dl = new QLabel(desc, widget);
        dl->setStyleSheet("font-size: 11px; color: #888;");
        dl->setWordWrap(true);
        info->addWidget(nl);
        info->addWidget(dl);
        hlay->addLayout(info, 1);

        auto* btn = new QPushButton("\xD0\xA3\xD1\x81\xD1\x82.", widget);
        btn->setFixedWidth(60);
        connect(btn, &QPushButton::clicked, this, [this, name]() {
            installPackage(name, false);
        });
        hlay->addWidget(btn);

        auto* item = new QListWidgetItem();
        item->setSizeHint(widget->sizeHint());
        _pacList->addItem(item);
        _pacList->setItemWidget(item, widget);
    }

    _status->setText(QString("\xD0\x9D\xD0\xB0\xD0\xB9\xD0\xB4\xD0\xB5\xD0\xBD\xD0\xBE: %1").arg(_pacList->count()));
}

void AppManager::searchAur() {
    QString query = _aurSearch->text().trimmed();
    if (query.isEmpty()) return;

    _aurList->clear();
    _status->setText("\xD0\x9F\xD0\xBE\xD0\xB8\xD1\x81\xD0\xBA \xD0\xB2 AUR...");

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
        _status->setText("\xD0\x9D\xD0\xB5\xD1\x82 AUR-\xD1\x85\xD0\xB5\xD0\xBB\xD0\xBF\xD0\xB5\xD1\x80\xD0\xB0 (yay/paru)");
        return;
    }

    QProcess proc;
    proc.start(helper, {"-Ss", query});
    proc.waitForFinished(10000);
    QString out = proc.readAllStandardOutput();

    auto lines = out.split('\n');
    for (int i = 0; i + 1 < lines.size(); i += 2) {
        QString header = lines[i].trimmed();
        QString desc   = lines[i + 1].trimmed();
        if (header.isEmpty()) continue;

        auto parts = header.split(' ');
        QString fullName = parts.value(0);
        QString name = fullName.contains('/') ? fullName.split('/').last() : fullName;

        auto* widget = new QWidget(_aurList);
        auto* hlay = new QHBoxLayout(widget);
        hlay->setContentsMargins(8, 4, 8, 4);

        auto* info = new QVBoxLayout;
        auto* nl = new QLabel(name, widget);
        nl->setStyleSheet("font-weight: bold;");
        auto* dl = new QLabel(desc, widget);
        dl->setStyleSheet("font-size: 11px; color: #888;");
        dl->setWordWrap(true);
        info->addWidget(nl);
        info->addWidget(dl);
        hlay->addLayout(info, 1);

        auto* btn = new QPushButton("\xD0\xA3\xD1\x81\xD1\x82.", widget);
        btn->setFixedWidth(60);
        connect(btn, &QPushButton::clicked, this, [this, name]() {
            installPackage(name, true);
        });
        hlay->addWidget(btn);

        auto* item = new QListWidgetItem();
        item->setSizeHint(widget->sizeHint());
        _aurList->addItem(item);
        _aurList->setItemWidget(item, widget);
    }

    _status->setText(QString("\xD0\x9D\xD0\xB0\xD0\xB9\xD0\xB4\xD0\xB5\xD0\xBD\xD0\xBE: %1").arg(_aurList->count()));
}

void AppManager::installPackage(const QString& name, bool aur) {
    if (aur) {
        QProcess test;
        test.start("which", {"yay"});
        test.waitForFinished(1000);
        QString helper = (test.exitCode() == 0) ? "yay" : "paru";
        runCmd(QString("pkexec %1 -S --noconfirm %2").arg(helper, name),
               "\xD0\xA3\xD1\x81\xD1\x82\xD0\xB0\xD0\xBD\xD0\xBE\xD0\xB2\xD0\xBA\xD0\xB0 " + name + "...");
    } else {
        runCmd(QString("pkexec pacman -S --noconfirm %1").arg(name),
               "\xD0\xA3\xD1\x81\xD1\x82\xD0\xB0\xD0\xBD\xD0\xBE\xD0\xB2\xD0\xBA\xD0\xB0 " + name + "...");
    }
}

void AppManager::removePackage(const QString& name) {
    runCmd(QString("pkexec pacman -R --noconfirm %1").arg(name),
           "\xD0\xA3\xD0\xB4\xD0\xB0\xD0\xBB\xD0\xB5\xD0\xBD\xD0\xB8\xD0\xB5 " + name + "...");
}

void AppManager::runCmd(const QString& cmd, const QString& statusMsg) {
    _status->setText(statusMsg);
    _proc->start("sh", {"-c", cmd});
}

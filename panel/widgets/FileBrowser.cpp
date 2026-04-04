#include "FileBrowser.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>

FileBrowser::FileBrowser(QWidget* parent) : QWidget(parent) {
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(10, 10, 10, 10);
    lay->setSpacing(8);

    auto* nav = new QHBoxLayout;
    _btnUp = new QPushButton("\xE2\xAC\x86", this);
    _btnUp->setFixedSize(32, 32);
    _btnUp->setToolTip("\xD0\x9D\xD0\xB0\xD0\xB7\xD0\xB0\xD0\xB4");
    _btnHome = new QPushButton("\xF0\x9F\x8F\xA0", this);
    _btnHome->setFixedSize(32, 32);
    _btnHome->setToolTip("\xD0\x94\xD0\xBE\xD0\xBC\xD0\xBE\xD0\xB9");
    _pathEdit = new QLineEdit(this);
    _pathEdit->setPlaceholderText("\xD0\x9F\xD1\x83\xD1\x82\xD1\x8C...");
    nav->addWidget(_btnUp);
    nav->addWidget(_btnHome);
    nav->addWidget(_pathEdit, 1);
    lay->addLayout(nav);

    _list = new QListWidget(this);
    _list->setIconSize(QSize(20, 20));
    lay->addWidget(_list, 1);

    _status = new QLabel(this);
    _status->setStyleSheet("font-size: 11px; color: #666;");
    lay->addWidget(_status);

    connect(_btnUp, &QPushButton::clicked, this, &FileBrowser::goUp);
    connect(_btnHome, &QPushButton::clicked, this, &FileBrowser::goHome);
    connect(_pathEdit, &QLineEdit::returnPressed, this, [this]() {
        navigate(_pathEdit->text());
    });
    connect(_list, &QListWidget::itemDoubleClicked, this, &FileBrowser::onItemDoubleClicked);

    _currentPath = QDir::homePath();
    refresh();
}

void FileBrowser::navigate(const QString& path) {
    QFileInfo fi(path);
    if (fi.isDir()) {
        _currentPath = fi.absoluteFilePath();
        refresh();
    }
}

void FileBrowser::refresh() {
    _list->clear();
    _pathEdit->setText(_currentPath);

    QDir dir(_currentPath);
    if (!dir.exists()) {
        _status->setText("\xD0\x9F\xD0\xB0\xD0\xBF\xD0\xBA\xD0\xB0 \xD0\xBD\xD0\xB5 \xD0\xBD\xD0\xB0\xD0\xB9\xD0\xB4\xD0\xB5\xD0\xBD\xD0\xB0");
        return;
    }

    dir.setFilter(QDir::AllEntries | QDir::NoDot | QDir::Hidden);
    dir.setSorting(QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    auto entries = dir.entryInfoList();
    int dirs = 0, files = 0;

    for (const auto& entry : entries) {
        QString name = entry.fileName();
        if (name == ".") continue;

        bool isDir = entry.isDir();
        QString icon = fileIcon(name, isDir);
        QString display = icon + "  " + name;

        if (isDir) {
            display += "/";
            dirs++;
        } else {
            files++;
            qint64 sz = entry.size();
            if (sz < 1024)
                display += QString("  (%1 B)").arg(sz);
            else if (sz < 1024 * 1024)
                display += QString("  (%1 KB)").arg(sz / 1024);
            else
                display += QString("  (%1 MB)").arg(sz / (1024 * 1024));
        }

        auto* item = new QListWidgetItem(display);
        item->setData(Qt::UserRole, entry.absoluteFilePath());
        item->setData(Qt::UserRole + 1, isDir);
        _list->addItem(item);
    }

    _status->setText(QString("%1 \xD0\xBF\xD0\xB0\xD0\xBF\xD0\xBE\xD0\xBA, %2 \xD1\x84\xD0\xB0\xD0\xB9\xD0\xBB\xD0\xBE\xD0\xB2").arg(dirs).arg(files));
}

void FileBrowser::onItemDoubleClicked(QListWidgetItem* item) {
    QString path = item->data(Qt::UserRole).toString();
    bool isDir = item->data(Qt::UserRole + 1).toBool();

    if (isDir) {
        navigate(path);
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

void FileBrowser::goUp() {
    QDir dir(_currentPath);
    if (dir.cdUp()) navigate(dir.absolutePath());
}

void FileBrowser::goHome() {
    navigate(QDir::homePath());
}

QString FileBrowser::fileIcon(const QString& name, bool isDir) const {
    if (isDir) return "\xF0\x9F\x93\x81";
    QString lower = name.toLower();
    if (lower.endsWith(".jpg") || lower.endsWith(".jpeg") ||
        lower.endsWith(".png") || lower.endsWith(".gif") ||
        lower.endsWith(".webp") || lower.endsWith(".svg"))
        return "\xF0\x9F\x96\xBC";
    if (lower.endsWith(".mp3") || lower.endsWith(".flac") ||
        lower.endsWith(".ogg") || lower.endsWith(".wav") ||
        lower.endsWith(".opus"))
        return "\xF0\x9F\x8E\xB5";
    if (lower.endsWith(".mp4") || lower.endsWith(".mkv") ||
        lower.endsWith(".avi") || lower.endsWith(".webm"))
        return "\xF0\x9F\x8E\xAC";
    if (lower.endsWith(".cpp") || lower.endsWith(".h") ||
        lower.endsWith(".py") || lower.endsWith(".sh") ||
        lower.endsWith(".js") || lower.endsWith(".rs"))
        return "\xF0\x9F\x93\x9D";
    if (lower.endsWith(".zip") || lower.endsWith(".tar") ||
        lower.endsWith(".gz") || lower.endsWith(".xz") ||
        lower.endsWith(".zst"))
        return "\xF0\x9F\x93\xA6";
    return "\xF0\x9F\x93\x84";
}

#include "WallpaperPicker.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QPixmap>
#include <QProcess>
#include <QStandardPaths>

WallpaperPicker::WallpaperPicker(QWidget* parent) : QWidget(parent) {
    _userDir    = QDir::homePath() + "/.local/share/nemac/wallpapers";
    _defaultDir = "/usr/share/nemac/wallpapers";

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(10, 10, 10, 10);
    lay->setSpacing(8);

    auto* title = new QLabel("\xF0\x9F\x96\xBC  \xD0\x9E\xD0\xB1\xD0\xBE\xD0\xB8", this);
    title->setStyleSheet("font-size: 16px; font-weight: bold;");
    lay->addWidget(title);

    _preview = new QLabel(this);
    _preview->setFixedHeight(140);
    _preview->setAlignment(Qt::AlignCenter);
    _preview->setStyleSheet("background: rgba(255,255,255,0.04); border-radius: 10px;");
    _preview->setScaledContents(false);
    lay->addWidget(_preview);

    _list = new QListWidget(this);
    lay->addWidget(_list, 1);
    connect(_list, &QListWidget::itemClicked, this, &WallpaperPicker::onItemClicked);

    auto* btns = new QHBoxLayout;
    _btnAdd = new QPushButton("+ \xD0\x94\xD0\xBE\xD0\xB1\xD0\xB0\xD0\xB2\xD0\xB8\xD1\x82\xD1\x8C", this);
    _btnApply = new QPushButton("\xD0\xA3\xD1\x81\xD1\x82\xD0\xB0\xD0\xBD\xD0\xBE\xD0\xB2\xD0\xB8\xD1\x82\xD1\x8C", this);
    _btnApply->setStyleSheet("background: rgba(136,170,255,0.2); font-weight: bold;");
    _btnApply->setEnabled(false);
    btns->addWidget(_btnAdd);
    btns->addStretch();
    btns->addWidget(_btnApply);
    lay->addLayout(btns);

    _status = new QLabel(this);
    _status->setStyleSheet("font-size: 11px; color: #666;");
    lay->addWidget(_status);

    connect(_btnAdd, &QPushButton::clicked, this, &WallpaperPicker::onAddWallpaper);
    connect(_btnApply, &QPushButton::clicked, this, [this]() {
        if (!_selected.isEmpty()) applyWallpaper(_selected);
    });

    refreshList();
}

void WallpaperPicker::refreshList() {
    _list->clear();
    scanDir(_defaultDir);
    scanDir(_userDir);
    _status->setText(QString("%1 \xD0\xBE\xD0\xB1\xD0\xBE\xD0\xB5\xD0\xB2").arg(_list->count()));
}

void WallpaperPicker::scanDir(const QString& dir) {
    QDir d(dir);
    if (!d.exists()) return;
    QStringList filters = {"*.jpg", "*.jpeg", "*.png", "*.webp"};
    auto files = d.entryInfoList(filters, QDir::Files, QDir::Name);
    for (const auto& f : files) {
        auto* item = new QListWidgetItem("\xF0\x9F\x96\xBC  " + f.fileName());
        item->setData(Qt::UserRole, f.absoluteFilePath());
        _list->addItem(item);
    }
}

void WallpaperPicker::onItemClicked(QListWidgetItem* item) {
    _selected = item->data(Qt::UserRole).toString();
    _btnApply->setEnabled(true);

    QPixmap pm(_selected);
    if (!pm.isNull()) {
        _preview->setPixmap(pm.scaled(_preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void WallpaperPicker::onAddWallpaper() {
    QString file = QFileDialog::getOpenFileName(this,
        "\xD0\x92\xD1\x8B\xD0\xB1\xD1\x80\xD0\xB0\xD1\x82\xD1\x8C \xD0\xBE\xD0\xB1\xD0\xBE\xD0\xB8",
        QDir::homePath(),
        "\xD0\x98\xD0\xB7\xD0\xBE\xD0\xB1\xD1\x80\xD0\xB0\xD0\xB6\xD0\xB5\xD0\xBD\xD0\xB8\xD1\x8F (*.jpg *.jpeg *.png *.webp)");
    if (file.isEmpty()) return;

    QDir().mkpath(_userDir);
    QFileInfo fi(file);
    QString dest = _userDir + "/" + fi.fileName();
    QFile::copy(file, dest);
    refreshList();
    _status->setText("\xD0\x94\xD0\xBE\xD0\xB1\xD0\xB0\xD0\xB2\xD0\xBB\xD0\xB5\xD0\xBD\xD0\xBE: " + fi.fileName());
}

void WallpaperPicker::applyWallpaper(const QString& path) {
    QProcess::startDetached("sh", {"-c",
        QString("feh --bg-fill '%1' 2>/dev/null || "
                "xwallpaper --zoom '%1' 2>/dev/null || "
                "display -window root '%1' 2>/dev/null").arg(path)});
    _status->setText("\xD0\xA3\xD1\x81\xD1\x82\xD0\xB0\xD0\xBD\xD0\xBE\xD0\xB2\xD0\xBB\xD0\xB5\xD0\xBD\xD0\xBE: " + QFileInfo(path).fileName());
}

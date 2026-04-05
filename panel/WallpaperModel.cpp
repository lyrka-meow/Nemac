#include "WallpaperModel.h"
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QFile>
#include <QProcess>

WallpaperModel::WallpaperModel(QObject* parent) : QAbstractListModel(parent) {
    _userDir    = QDir::homePath() + "/.local/share/nemac/wallpapers";
    _defaultDir = "/usr/share/nemac/wallpapers";
    refresh();
}

int WallpaperModel::rowCount(const QModelIndex&) const {
    return _paths.size();
}

QVariant WallpaperModel::data(const QModelIndex& idx, int role) const {
    if (idx.row() < 0 || idx.row() >= _paths.size()) return {};
    switch (role) {
    case NameRole:     return _names[idx.row()];
    case FullPathRole: return _paths[idx.row()];
    }
    return {};
}

QHash<int, QByteArray> WallpaperModel::roleNames() const {
    return {
        {NameRole,     "name"},
        {FullPathRole, "fullPath"},
    };
}

void WallpaperModel::scanDir(const QString& dir) {
    QDir d(dir);
    if (!d.exists()) return;
    QStringList filters = {"*.jpg", "*.jpeg", "*.png", "*.webp"};
    auto files = d.entryInfoList(filters, QDir::Files, QDir::Name);
    for (const auto& f : files) {
        _paths.append(f.absoluteFilePath());
        _names.append(f.fileName());
    }
}

void WallpaperModel::refresh() {
    beginResetModel();
    _paths.clear();
    _names.clear();
    scanDir(_defaultDir);
    scanDir(_userDir);
    endResetModel();
    emit listChanged();
}

void WallpaperModel::select(int index) {
    if (index < 0 || index >= _paths.size()) return;
    _selected = _paths[index];
    emit selectionChanged();
}

void WallpaperModel::apply() {
    if (_selected.isEmpty()) return;
    QProcess::startDetached("sh", {"-c",
        QString("feh --bg-fill '%1' 2>/dev/null || "
                "xwallpaper --zoom '%1' 2>/dev/null || "
                "display -window root '%1' 2>/dev/null").arg(_selected)});
    emit statusText(QString("\xD0\xA3\xD1\x81\xD1\x82\xD0\xB0\xD0\xBD\xD0\xBE\xD0\xB2\xD0\xBB\xD0\xB5\xD0\xBD\xD0\xBE: %1")
                    .arg(QFileInfo(_selected).fileName()));
}

void WallpaperModel::addDialog() {
    QString file = QFileDialog::getOpenFileName(nullptr,
        "\xD0\x92\xD1\x8B\xD0\xB1\xD1\x80\xD0\xB0\xD1\x82\xD1\x8C \xD0\xBE\xD0\xB1\xD0\xBE\xD0\xB8",
        QDir::homePath(),
        "\xD0\x98\xD0\xB7\xD0\xBE\xD0\xB1\xD1\x80\xD0\xB0\xD0\xB6\xD0\xB5\xD0\xBD\xD0\xB8\xD1\x8F (*.jpg *.jpeg *.png *.webp)");
    if (file.isEmpty()) return;

    QDir().mkpath(_userDir);
    QFileInfo fi(file);
    QString dest = _userDir + "/" + fi.fileName();
    QFile::copy(file, dest);
    refresh();
    emit statusText("\xD0\x94\xD0\xBE\xD0\xB1\xD0\xB0\xD0\xB2\xD0\xBB\xD0\xB5\xD0\xBD\xD0\xBE: " + fi.fileName());
}

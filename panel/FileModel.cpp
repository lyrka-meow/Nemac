#include "FileModel.h"
#include <QDir>
#include <QDesktopServices>
#include <QUrl>

FileModel::FileModel(QObject* parent) : QAbstractListModel(parent) {
    _path = QDir::homePath();
    reload();
}

int FileModel::rowCount(const QModelIndex&) const {
    return _entries.size();
}

QVariant FileModel::data(const QModelIndex& idx, int role) const {
    if (idx.row() < 0 || idx.row() >= _entries.size()) return {};
    const auto& fi = _entries[idx.row()];
    switch (role) {
    case NameRole:     return fi.fileName() + (fi.isDir() ? "/" : "");
    case FilePathRole: return fi.absoluteFilePath();
    case IsDirRole:    return fi.isDir();
    case IconRole:     return iconFor(fi.fileName(), fi.isDir());
    case SizeRole: {
        if (fi.isDir()) return "";
        qint64 sz = fi.size();
        if (sz < 1024) return QString("%1 B").arg(sz);
        if (sz < 1024 * 1024) return QString("%1 KB").arg(sz / 1024);
        return QString("%1 MB").arg(sz / (1024 * 1024));
    }
    }
    return {};
}

QHash<int, QByteArray> FileModel::roleNames() const {
    return {
        {NameRole,     "name"},
        {FilePathRole, "filePath"},
        {IsDirRole,    "isDir"},
        {IconRole,     "icon"},
        {SizeRole,     "sizeStr"},
    };
}

void FileModel::cd(const QString& path) {
    QFileInfo fi(path);
    if (!fi.isDir()) return;
    _path = fi.absoluteFilePath();
    reload();
    emit pathChanged();
}

void FileModel::cdUp() {
    QDir d(_path);
    if (d.cdUp()) {
        _path = d.absolutePath();
        reload();
        emit pathChanged();
    }
}

void FileModel::goHome() {
    _path = QDir::homePath();
    reload();
    emit pathChanged();
}

void FileModel::open(int index) {
    if (index < 0 || index >= _entries.size()) return;
    const auto& fi = _entries[index];
    if (fi.isDir()) {
        cd(fi.absoluteFilePath());
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absoluteFilePath()));
    }
}

void FileModel::reload() {
    beginResetModel();
    QDir dir(_path);
    dir.setFilter(QDir::AllEntries | QDir::NoDot | QDir::Hidden);
    dir.setSorting(QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    _entries = dir.entryInfoList();
    _dirs = 0; _files = 0;
    for (const auto& e : _entries) {
        if (e.isDir()) _dirs++; else _files++;
    }
    endResetModel();
    emit contentsChanged();
}

QString FileModel::iconFor(const QString& name, bool isDir) const {
    if (isDir) return "\xF0\x9F\x93\x81";
    QString l = name.toLower();
    if (l.endsWith(".jpg") || l.endsWith(".jpeg") || l.endsWith(".png") ||
        l.endsWith(".gif") || l.endsWith(".webp") || l.endsWith(".svg"))
        return "\xF0\x9F\x96\xBC";
    if (l.endsWith(".mp3") || l.endsWith(".flac") || l.endsWith(".ogg") ||
        l.endsWith(".wav") || l.endsWith(".opus"))
        return "\xF0\x9F\x8E\xB5";
    if (l.endsWith(".mp4") || l.endsWith(".mkv") || l.endsWith(".avi") ||
        l.endsWith(".webm"))
        return "\xF0\x9F\x8E\xAC";
    if (l.endsWith(".cpp") || l.endsWith(".h") || l.endsWith(".py") ||
        l.endsWith(".sh") || l.endsWith(".js") || l.endsWith(".rs"))
        return "\xF0\x9F\x93\x9D";
    if (l.endsWith(".zip") || l.endsWith(".tar") || l.endsWith(".gz") ||
        l.endsWith(".xz") || l.endsWith(".zst"))
        return "\xF0\x9F\x93\xA6";
    return "\xF0\x9F\x93\x84";
}

#pragma once
#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QString>

class WallpaperPicker : public QWidget {
    Q_OBJECT
public:
    explicit WallpaperPicker(QWidget* parent = nullptr);

private slots:
    void refreshList();
    void onItemClicked(QListWidgetItem* item);
    void onAddWallpaper();

private:
    void applyWallpaper(const QString& path);
    void scanDir(const QString& dir);

    QListWidget* _list     = nullptr;
    QLabel*      _preview  = nullptr;
    QLabel*      _status   = nullptr;
    QPushButton* _btnApply = nullptr;
    QPushButton* _btnAdd   = nullptr;
    QString      _selected;
    QString      _userDir;
    QString      _defaultDir;
};

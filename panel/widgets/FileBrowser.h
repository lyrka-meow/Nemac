#pragma once
#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QString>

class FileBrowser : public QWidget {
    Q_OBJECT
public:
    explicit FileBrowser(QWidget* parent = nullptr);

private slots:
    void navigate(const QString& path);
    void onItemDoubleClicked(QListWidgetItem* item);
    void goUp();
    void goHome();

private:
    void refresh();
    QString fileIcon(const QString& name, bool isDir) const;

    QLineEdit*   _pathEdit = nullptr;
    QListWidget* _list     = nullptr;
    QPushButton* _btnUp    = nullptr;
    QPushButton* _btnHome  = nullptr;
    QLabel*      _status   = nullptr;
    QString      _currentPath;
};

#pragma once
#include <QWidget>
#include <QTabWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QProcess>

class AppManager : public QWidget {
    Q_OBJECT
public:
    explicit AppManager(QWidget* parent = nullptr);

private slots:
    void searchPacman();
    void searchAur();
    void installPackage(const QString& name, bool aur);
    void removePackage(const QString& name);

private:
    void populateRecommended();
    void runCmd(const QString& cmd, const QString& statusMsg);

    QTabWidget*  _tabs       = nullptr;
    QListWidget* _recList    = nullptr;
    QListWidget* _pacList    = nullptr;
    QListWidget* _aurList    = nullptr;
    QLineEdit*   _pacSearch  = nullptr;
    QLineEdit*   _aurSearch  = nullptr;
    QLabel*      _status     = nullptr;
    QProcess*    _proc       = nullptr;
};

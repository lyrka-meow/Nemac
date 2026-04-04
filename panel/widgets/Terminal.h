#pragma once
#include <QWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QProcess>
#include <QLabel>

class Terminal : public QWidget {
    Q_OBJECT
public:
    explicit Terminal(QWidget* parent = nullptr);

    void setEnabled(bool on);
    bool isTerminalEnabled() const { return _enabled; }

private slots:
    void onCommand();
    void onOutput();
    void onFinished(int code, QProcess::ExitStatus status);

private:
    QPlainTextEdit* _output  = nullptr;
    QLineEdit*      _input   = nullptr;
    QLabel*         _prompt  = nullptr;
    QLabel*         _notice  = nullptr;
    QProcess*       _proc    = nullptr;
    bool            _enabled = false;
    QString         _cwd;
};

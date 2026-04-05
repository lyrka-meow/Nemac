#pragma once
#include <QObject>
#include <QProcess>
#include <QString>

class TermBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString output READ output NOTIFY outputChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString cwd READ cwd NOTIFY cwdChanged)
public:
    explicit TermBackend(QObject* parent = nullptr);

    QString output() const { return _output; }
    bool    busy()   const { return _busy; }
    QString cwd()    const { return _cwd; }

    Q_INVOKABLE void exec(const QString& cmd);
    Q_INVOKABLE void clear();

signals:
    void outputChanged();
    void busyChanged();
    void cwdChanged();

private:
    void appendLine(const QString& line);

    QProcess* _proc = nullptr;
    QString _output;
    QString _cwd;
    bool _busy = false;
};

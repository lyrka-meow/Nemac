#pragma once
#include <QObject>
#include <QVariantList>
#include <QProcess>

class PackageBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QVariantList recommended READ recommended CONSTANT)
public:
    explicit PackageBackend(QObject* parent = nullptr);

    QString status() const { return _status; }
    QVariantList recommended() const { return _recommended; }

    Q_INVOKABLE void searchPacman(const QString& query);
    Q_INVOKABLE void searchAur(const QString& query);
    Q_INVOKABLE void install(const QString& name, bool aur);

signals:
    void statusChanged();
    void resultsReady(const QVariantList& list);

private:
    QVariantList parseResults(const QString& output, bool aur);
    void setStatus(const QString& s);

    QString _status;
    QVariantList _recommended;
    QProcess* _proc = nullptr;
};

#pragma once
#include <QObject>
#include <QString>
#include <QDBusConnection>

class MprisClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY metadataChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY metadataChanged)
    Q_PROPERTY(QString artUrl READ artUrl NOTIFY metadataChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY statusChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)

public:
    explicit MprisClient(QObject* parent = nullptr);

    QString title() const { return _title; }
    QString artist() const { return _artist; }
    QString artUrl() const { return _artUrl; }
    bool playing() const { return _playing; }
    bool available() const { return _available; }

    void init();

    Q_INVOKABLE void playPause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();

signals:
    void metadataChanged();
    void statusChanged();
    void availableChanged();

private slots:
    void onServiceOwnerChanged(const QString& name, const QString& oldOwner,
                               const QString& newOwner);
    void onPropertiesChanged(const QString& iface, const QVariantMap& changed,
                             const QStringList& invalidated);

private:
    void findPlayer();
    void fetchMetadata();
    void fetchStatus();
    void connectPlayer();

    QDBusConnection _bus;
    bool _inited = false;
    QString _service;
    QString _title;
    QString _artist;
    QString _artUrl;
    bool _playing = false;
    bool _available = false;
};

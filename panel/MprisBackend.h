#pragma once
#include <QObject>
#include <QTimer>
#include <QString>
#include <dbus/dbus.h>

class MprisBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString track    READ track    NOTIFY changed)
    Q_PROPERTY(QString artist   READ artist   NOTIFY changed)
    Q_PROPERTY(QString cover    READ cover    NOTIFY changed)
    Q_PROPERTY(bool    playing  READ playing  NOTIFY changed)
    Q_PROPERTY(qint64  posUs    READ posUs    NOTIFY changed)
    Q_PROPERTY(qint64  durUs    READ durUs    NOTIFY changed)
    Q_PROPERTY(bool    active   READ active   NOTIFY changed)
public:
    explicit MprisBackend(QObject* parent = nullptr);
    ~MprisBackend() override;

    QString track()   const { return _track; }
    QString artist()  const { return _artist; }
    QString cover()   const { return _cover; }
    bool    playing() const { return _playing; }
    qint64  posUs()   const { return _posUs; }
    qint64  durUs()   const { return _durUs; }
    bool    active()  const { return _active; }

    Q_INVOKABLE void playPause();
    Q_INVOKABLE void next();
    Q_INVOKABLE void prev();
    Q_INVOKABLE void seek(double fraction);

signals:
    void changed();

private:
    void poll();
    bool fetch();
    void sendCmd(const char* method);
    static std::string dbStr(DBusMessageIter* it);

    DBusConnection* _dbus = nullptr;
    QTimer*  _timer   = nullptr;
    QString  _track;
    QString  _artist;
    QString  _cover;
    bool     _playing = false;
    qint64   _posUs   = 0;
    qint64   _durUs   = 0;
    bool     _active  = false;
    std::string _bus;
};

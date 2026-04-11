#include "mpris.h"
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusArgument>
#include <QStringList>
#include <QTimer>

static const char* MPRIS_PREFIX = "org.mpris.MediaPlayer2.";
static const char* PLAYER_IFACE = "org.mpris.MediaPlayer2.Player";
static const char* PROP_IFACE   = "org.freedesktop.DBus.Properties";

MprisClient::MprisClient(QObject* parent)
    : QObject(parent), _bus(QDBusConnection("_dummy_"))
{
}

void MprisClient::init() {
    _bus = QDBusConnection::sessionBus();
    _inited = true;
    _bus.connect("org.freedesktop.DBus", "/org/freedesktop/DBus",
                 "org.freedesktop.DBus", "NameOwnerChanged",
                 this, SLOT(onServiceOwnerChanged(QString,QString,QString)));
    findPlayer();
}

void MprisClient::findPlayer() {
    QDBusReply<QStringList> reply =
        _bus.interface()->registeredServiceNames();
    if (!reply.isValid()) return;

    QString best;
    for (const QString& name : reply.value()) {
        if (!name.startsWith(MPRIS_PREFIX)) continue;
        best = name;
        if (name.contains("chrom", Qt::CaseInsensitive) ||
            name.contains("firefox", Qt::CaseInsensitive))
            break;
    }

    if (best.isEmpty()) {
        if (_available) {
            _available = false;
            _title.clear();
            _artist.clear();
            _artUrl.clear();
            _playing = false;
            emit availableChanged();
            emit metadataChanged();
            emit statusChanged();
        }
        return;
    }

    if (best != _service) {
        _service = best;
        connectPlayer();
    }

    _available = true;
    emit availableChanged();
    fetchMetadata();
    fetchStatus();
}

void MprisClient::connectPlayer() {
    _bus.connect(_service, "/org/mpris/MediaPlayer2",
                 PROP_IFACE, "PropertiesChanged",
                 this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
}

void MprisClient::fetchMetadata() {
    QDBusMessage msg = QDBusMessage::createMethodCall(
        _service, "/org/mpris/MediaPlayer2", PROP_IFACE, "Get");
    msg << PLAYER_IFACE << "Metadata";

    QDBusReply<QVariant> reply = _bus.call(msg);
    if (!reply.isValid()) return;

    QVariantMap meta;
    QVariant v = reply.value();
    if (v.canConvert<QDBusArgument>()) {
        const QDBusArgument arg = v.value<QDBusArgument>();
        arg >> meta;
    } else {
        meta = v.toMap();
    }

    QString t = meta.value("xesam:title").toString();
    QStringList artists = meta.value("xesam:artist").toStringList();
    QString a = artists.isEmpty() ? QString() : artists.first();
    QString art = meta.value("mpris:artUrl").toString();

    if (t != _title || a != _artist || art != _artUrl) {
        _title = t;
        _artist = a;
        _artUrl = art;
        emit metadataChanged();
    }
}

void MprisClient::fetchStatus() {
    QDBusMessage msg = QDBusMessage::createMethodCall(
        _service, "/org/mpris/MediaPlayer2", PROP_IFACE, "Get");
    msg << PLAYER_IFACE << "PlaybackStatus";

    QDBusReply<QVariant> reply = _bus.call(msg);
    if (!reply.isValid()) return;

    bool p = (reply.value().toString() == "Playing");
    if (p != _playing) {
        _playing = p;
        emit statusChanged();
    }
}

void MprisClient::playPause() {
    if (!_inited || _service.isEmpty()) return;
    QDBusMessage msg = QDBusMessage::createMethodCall(
        _service, "/org/mpris/MediaPlayer2", PLAYER_IFACE, "PlayPause");
    _bus.call(msg, QDBus::NoBlock);
}

void MprisClient::next() {
    if (!_inited || _service.isEmpty()) return;
    QDBusMessage msg = QDBusMessage::createMethodCall(
        _service, "/org/mpris/MediaPlayer2", PLAYER_IFACE, "Next");
    _bus.call(msg, QDBus::NoBlock);
}

void MprisClient::previous() {
    if (!_inited || _service.isEmpty()) return;
    QDBusMessage msg = QDBusMessage::createMethodCall(
        _service, "/org/mpris/MediaPlayer2", PLAYER_IFACE, "Previous");
    _bus.call(msg, QDBus::NoBlock);
}

void MprisClient::onServiceOwnerChanged(const QString& name,
                                         const QString&,
                                         const QString& newOwner) {
    if (!name.startsWith(MPRIS_PREFIX)) return;
    if (newOwner.isEmpty() && name == _service) {
        _service.clear();
        findPlayer();
    } else if (!newOwner.isEmpty()) {
        findPlayer();
    }
}

void MprisClient::onPropertiesChanged(const QString& iface,
                                       const QVariantMap& changed,
                                       const QStringList&) {
    if (iface != PLAYER_IFACE) return;
    if (changed.contains("Metadata"))
        fetchMetadata();
    if (changed.contains("PlaybackStatus"))
        fetchStatus();
}

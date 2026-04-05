#include "MprisBackend.h"
#include <cstring>

MprisBackend::MprisBackend(QObject* parent) : QObject(parent) {
    DBusError err;
    dbus_error_init(&err);
    _dbus = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        dbus_error_free(&err);
        _dbus = nullptr;
    }
    if (_dbus) dbus_connection_set_exit_on_disconnect(_dbus, FALSE);

    _timer = new QTimer(this);
    connect(_timer, &QTimer::timeout, this, &MprisBackend::poll);
    _timer->start(2000);
    poll();
}

MprisBackend::~MprisBackend() {
    if (_dbus) { dbus_connection_unref(_dbus); _dbus = nullptr; }
}

std::string MprisBackend::dbStr(DBusMessageIter* it) {
    const char* s = nullptr;
    dbus_message_iter_get_basic(it, &s);
    return s ? std::string(s) : std::string();
}

bool MprisBackend::fetch() {
    if (!_dbus) return false;

    DBusMessage* msg = dbus_message_new_method_call(
        "org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", "ListNames");
    if (!msg) return false;
    DBusError err; dbus_error_init(&err);
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(_dbus, msg, 30, &err);
    dbus_message_unref(msg);
    if (dbus_error_is_set(&err)) { dbus_error_free(&err); return false; }
    if (!reply) return false;

    std::string bus;
    DBusMessageIter it;
    dbus_message_iter_init(reply, &it);
    if (dbus_message_iter_get_arg_type(&it) == DBUS_TYPE_ARRAY) {
        DBusMessageIter sub;
        dbus_message_iter_recurse(&it, &sub);
        while (dbus_message_iter_get_arg_type(&sub) == DBUS_TYPE_STRING) {
            std::string name = dbStr(&sub);
            if (name.rfind("org.mpris.MediaPlayer2.", 0) == 0) { bus = name; break; }
            dbus_message_iter_next(&sub);
        }
    }
    dbus_message_unref(reply);
    if (bus.empty()) return false;

    msg = dbus_message_new_method_call(bus.c_str(), "/org/mpris/MediaPlayer2",
                                       "org.freedesktop.DBus.Properties", "GetAll");
    if (!msg) return false;
    const char* iface = "org.mpris.MediaPlayer2.Player";
    dbus_message_append_args(msg, DBUS_TYPE_STRING, &iface, DBUS_TYPE_INVALID);
    dbus_error_init(&err);
    reply = dbus_connection_send_with_reply_and_block(_dbus, msg, 30, &err);
    dbus_message_unref(msg);
    if (dbus_error_is_set(&err)) { dbus_error_free(&err); return false; }
    if (!reply) return false;

    std::string track, artist, cover;
    bool playing = false;
    int64_t pos = 0, dur = 0;

    DBusMessageIter pi;
    dbus_message_iter_init(reply, &pi);
    if (dbus_message_iter_get_arg_type(&pi) == DBUS_TYPE_ARRAY) {
        DBusMessageIter arr;
        dbus_message_iter_recurse(&pi, &arr);
        while (dbus_message_iter_get_arg_type(&arr) == DBUS_TYPE_DICT_ENTRY) {
            DBusMessageIter entry;
            dbus_message_iter_recurse(&arr, &entry);
            std::string key = dbStr(&entry);
            dbus_message_iter_next(&entry);
            if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_VARIANT) {
                dbus_message_iter_next(&arr); continue;
            }
            DBusMessageIter var;
            dbus_message_iter_recurse(&entry, &var);

            if (key == "PlaybackStatus") {
                std::string st = dbStr(&var);
                playing = (st == "Playing");
            } else if (key == "Position" &&
                       dbus_message_iter_get_arg_type(&var) == DBUS_TYPE_INT64) {
                dbus_message_iter_get_basic(&var, &pos);
            } else if (key == "Metadata" &&
                       dbus_message_iter_get_arg_type(&var) == DBUS_TYPE_ARRAY) {
                DBusMessageIter meta;
                dbus_message_iter_recurse(&var, &meta);
                while (dbus_message_iter_get_arg_type(&meta) == DBUS_TYPE_DICT_ENTRY) {
                    DBusMessageIter me;
                    dbus_message_iter_recurse(&meta, &me);
                    std::string mk = dbStr(&me);
                    dbus_message_iter_next(&me);
                    if (dbus_message_iter_get_arg_type(&me) != DBUS_TYPE_VARIANT) {
                        dbus_message_iter_next(&meta); continue;
                    }
                    DBusMessageIter mv;
                    dbus_message_iter_recurse(&me, &mv);
                    if (mk == "xesam:title" && dbus_message_iter_get_arg_type(&mv) == DBUS_TYPE_STRING)
                        track = dbStr(&mv);
                    else if (mk == "xesam:artist" && dbus_message_iter_get_arg_type(&mv) == DBUS_TYPE_ARRAY) {
                        DBusMessageIter as;
                        dbus_message_iter_recurse(&mv, &as);
                        if (dbus_message_iter_get_arg_type(&as) == DBUS_TYPE_STRING)
                            artist = dbStr(&as);
                    }
                    else if (mk == "mpris:artUrl" && dbus_message_iter_get_arg_type(&mv) == DBUS_TYPE_STRING)
                        cover = dbStr(&mv);
                    else if (mk == "mpris:length" && dbus_message_iter_get_arg_type(&mv) == DBUS_TYPE_INT64)
                        dbus_message_iter_get_basic(&mv, &dur);
                    dbus_message_iter_next(&meta);
                }
            }
            dbus_message_iter_next(&arr);
        }
    }
    dbus_message_unref(reply);

    _bus     = bus;
    _track   = QString::fromStdString(track);
    _artist  = QString::fromStdString(artist);
    _playing = playing;
    _posUs   = pos;
    _durUs   = dur;

    std::string cpath = cover;
    if (cpath.rfind("file://", 0) == 0) cpath = cpath.substr(7);
    _cover = QString::fromStdString(cpath);

    _active = true;
    return true;
}

void MprisBackend::poll() {
    if (!fetch()) {
        _active = false;
        _track.clear();
        _artist.clear();
        _cover.clear();
        _playing = false;
        _posUs = 0;
        _durUs = 0;
        _bus.clear();
    }
    emit changed();
}

void MprisBackend::sendCmd(const char* method) {
    if (!_dbus || _bus.empty()) return;
    DBusMessage* m = dbus_message_new_method_call(
        _bus.c_str(), "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player", method);
    if (!m) return;
    dbus_connection_send(_dbus, m, nullptr);
    dbus_message_unref(m);
    dbus_connection_flush(_dbus);
}

void MprisBackend::playPause() {
    sendCmd("PlayPause");
    _playing = !_playing;
    emit changed();
}

void MprisBackend::next() {
    sendCmd("Next");
    QTimer::singleShot(300, this, &MprisBackend::poll);
}

void MprisBackend::prev() {
    sendCmd("Previous");
    QTimer::singleShot(300, this, &MprisBackend::poll);
}

void MprisBackend::seek(double fraction) {
    if (!_dbus || _bus.empty() || _durUs <= 0) return;
    int64_t pos = (int64_t)(fraction * _durUs);
    DBusMessage* m = dbus_message_new_method_call(
        _bus.c_str(), "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player", "SetPosition");
    if (!m) return;
    const char* path = "/";
    dbus_message_append_args(m, DBUS_TYPE_OBJECT_PATH, &path,
                             DBUS_TYPE_INT64, &pos, DBUS_TYPE_INVALID);
    dbus_connection_send(_dbus, m, nullptr);
    dbus_message_unref(m);
    dbus_connection_flush(_dbus);
    _posUs = pos;
    emit changed();
}

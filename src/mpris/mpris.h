#pragma once
#include <dbus/dbus.h>
#include <string>
#include <cstring>
#include <cstdint>
#include <cstdio>

struct MprisInfo {
    bool        valid       = false;
    bool        playing     = false;
    std::string title;
    std::string artist;
    std::string art_url;
    int64_t     length_us   = 0;
    int64_t     position_us = 0;
    std::string player_bus;
};

static inline std::string dbus_str(DBusMessageIter* it) {
    const char* s = nullptr;
    dbus_message_iter_get_basic(it, &s);
    return s ? std::string(s) : std::string();
}

static inline DBusMessage* dbus_call(DBusConnection* c,
                                     const char* dest, const char* path,
                                     const char* iface, const char* method,
                                     int timeout_ms = 30) {
    DBusMessage* m = dbus_message_new_method_call(dest, path, iface, method);
    if (!m) return nullptr;
    DBusError err; dbus_error_init(&err);
    DBusMessage* r = dbus_connection_send_with_reply_and_block(c, m, timeout_ms, &err);
    dbus_message_unref(m);
    if (dbus_error_is_set(&err)) { dbus_error_free(&err); return nullptr; }
    return r;
}

static inline void parse_variant_string(DBusMessageIter* var, std::string& out) {
    if (dbus_message_iter_get_arg_type(var) == DBUS_TYPE_STRING)
        out = dbus_str(var);
}

static inline void parse_variant_array_string(DBusMessageIter* var, std::string& out) {
    if (dbus_message_iter_get_arg_type(var) != DBUS_TYPE_ARRAY) return;
    DBusMessageIter sub; dbus_message_iter_recurse(var, &sub);
    if (dbus_message_iter_get_arg_type(&sub) == DBUS_TYPE_STRING)
        out = dbus_str(&sub);
}

static inline void parse_metadata_dict(DBusMessageIter* dict, MprisInfo& info) {
    while (dbus_message_iter_get_arg_type(dict) == DBUS_TYPE_DICT_ENTRY) {
        DBusMessageIter entry; dbus_message_iter_recurse(dict, &entry);
        std::string key = dbus_str(&entry);
        dbus_message_iter_next(&entry);
        if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_VARIANT) {
            dbus_message_iter_next(dict); continue;
        }
        DBusMessageIter var; dbus_message_iter_recurse(&entry, &var);
        if (key == "xesam:title")  parse_variant_string(&var, info.title);
        else if (key == "xesam:artist") parse_variant_array_string(&var, info.artist);
        else if (key == "mpris:artUrl") parse_variant_string(&var, info.art_url);
        else if (key == "mpris:length" &&
                 dbus_message_iter_get_arg_type(&var) == DBUS_TYPE_INT64)
            dbus_message_iter_get_basic(&var, &info.length_us);
        dbus_message_iter_next(dict);
    }
}

inline DBusConnection* mpris_connect() {
    DBusError err; dbus_error_init(&err);
    DBusConnection* c = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) { dbus_error_free(&err); return nullptr; }
    dbus_connection_set_exit_on_disconnect(c, FALSE);
    return c;
}

inline bool mpris_poll(DBusConnection* conn, MprisInfo& out) {
    out = {};
    if (!conn) return false;

    DBusMessage* reply = dbus_call(conn,
        "org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", "ListNames");
    if (!reply) return false;

    std::string player_bus;
    DBusMessageIter it; dbus_message_iter_init(reply, &it);
    if (dbus_message_iter_get_arg_type(&it) == DBUS_TYPE_ARRAY) {
        DBusMessageIter sub; dbus_message_iter_recurse(&it, &sub);
        while (dbus_message_iter_get_arg_type(&sub) == DBUS_TYPE_STRING) {
            std::string name = dbus_str(&sub);
            if (name.rfind("org.mpris.MediaPlayer2.", 0) == 0) {
                player_bus = name; break;
            }
            dbus_message_iter_next(&sub);
        }
    }
    dbus_message_unref(reply);
    if (player_bus.empty()) return false;

    DBusMessage* msg = dbus_message_new_method_call(
        player_bus.c_str(),
        "/org/mpris/MediaPlayer2",
        "org.freedesktop.DBus.Properties",
        "GetAll");
    if (!msg) return false;
    const char* iface = "org.mpris.MediaPlayer2.Player";
    dbus_message_append_args(msg, DBUS_TYPE_STRING, &iface, DBUS_TYPE_INVALID);
    DBusError err; dbus_error_init(&err);
    DBusMessage* props = dbus_connection_send_with_reply_and_block(conn, msg, 30, &err);
    dbus_message_unref(msg);
    if (dbus_error_is_set(&err)) { dbus_error_free(&err); return false; }
    if (!props) return false;

    DBusMessageIter pi; dbus_message_iter_init(props, &pi);
    if (dbus_message_iter_get_arg_type(&pi) == DBUS_TYPE_ARRAY) {
        DBusMessageIter arr; dbus_message_iter_recurse(&pi, &arr);
        while (dbus_message_iter_get_arg_type(&arr) == DBUS_TYPE_DICT_ENTRY) {
            DBusMessageIter entry; dbus_message_iter_recurse(&arr, &entry);
            std::string key = dbus_str(&entry);
            dbus_message_iter_next(&entry);
            if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_VARIANT) {
                dbus_message_iter_next(&arr); continue;
            }
            DBusMessageIter var; dbus_message_iter_recurse(&entry, &var);

            if (key == "PlaybackStatus") {
                std::string st = dbus_str(&var);
                out.playing = (st == "Playing");
            } else if (key == "Position" &&
                       dbus_message_iter_get_arg_type(&var) == DBUS_TYPE_INT64) {
                dbus_message_iter_get_basic(&var, &out.position_us);
            } else if (key == "Metadata" &&
                       dbus_message_iter_get_arg_type(&var) == DBUS_TYPE_ARRAY) {
                DBusMessageIter meta; dbus_message_iter_recurse(&var, &meta);
                parse_metadata_dict(&meta, out);
            }
            dbus_message_iter_next(&arr);
        }
    }
    dbus_message_unref(props);

    out.valid      = true;
    out.player_bus = player_bus;
    return true;
}

inline void mpris_play_pause(DBusConnection* c, const std::string& bus) {
    DBusMessage* m = dbus_message_new_method_call(bus.c_str(),
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player", "PlayPause");
    if (!m) return;
    dbus_connection_send(c, m, nullptr);
    dbus_message_unref(m);
    dbus_connection_flush(c);
}

inline void mpris_next(DBusConnection* c, const std::string& bus) {
    DBusMessage* m = dbus_message_new_method_call(bus.c_str(),
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player", "Next");
    if (!m) return;
    dbus_connection_send(c, m, nullptr);
    dbus_message_unref(m);
    dbus_connection_flush(c);
}

inline void mpris_prev(DBusConnection* c, const std::string& bus) {
    DBusMessage* m = dbus_message_new_method_call(bus.c_str(),
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player", "Previous");
    if (!m) return;
    dbus_connection_send(c, m, nullptr);
    dbus_message_unref(m);
    dbus_connection_flush(c);
}

inline void mpris_seek_abs(DBusConnection* c, const std::string& bus, int64_t pos_us) {
    DBusMessage* m = dbus_message_new_method_call(bus.c_str(),
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player", "SetPosition");
    if (!m) return;
    const char* dummy_id = "/";
    dbus_message_append_args(m,
        DBUS_TYPE_OBJECT_PATH, &dummy_id,
        DBUS_TYPE_INT64, &pos_us,
        DBUS_TYPE_INVALID);
    dbus_connection_send(c, m, nullptr);
    dbus_message_unref(m);
    dbus_connection_flush(c);
}

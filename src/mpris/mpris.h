#pragma once
#include <dbus/dbus.h>
#include <string>
#include <cstdint>

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

DBusConnection* mpris_connect();
bool mpris_poll(DBusConnection* conn, MprisInfo& out);
void mpris_play_pause(DBusConnection* c, const std::string& bus);
void mpris_next(DBusConnection* c, const std::string& bus);
void mpris_prev(DBusConnection* c, const std::string& bus);
void mpris_seek_abs(DBusConnection* c, const std::string& bus, int64_t pos_us);

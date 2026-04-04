#include "MediaPlayer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QFile>
#include <QUrl>

static std::string dbus_str(DBusMessageIter* it) {
    const char* s = nullptr;
    dbus_message_iter_get_basic(it, &s);
    return s ? std::string(s) : std::string();
}

static QString fmt_us(int64_t us) {
    if (us < 0) us = 0;
    int s = (int)(us / 1000000);
    return QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}

MediaPlayer::MediaPlayer(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    _art = new QLabel(this);
    _art->setFixedSize(120, 120);
    _art->setAlignment(Qt::AlignCenter);
    _art->setStyleSheet("background: rgba(255,255,255,0.04); border-radius: 12px;");
    _art->setScaledContents(true);

    _title = new QLabel(this);
    _title->setStyleSheet("font-size: 16px; font-weight: bold; color: #fff;");
    _title->setWordWrap(true);

    _artist = new QLabel(this);
    _artist->setStyleSheet("font-size: 13px; color: #aaa;");

    auto* top = new QHBoxLayout;
    top->setSpacing(14);
    top->addWidget(_art);
    auto* info = new QVBoxLayout;
    info->addStretch();
    info->addWidget(_title);
    info->addWidget(_artist);
    info->addStretch();
    top->addLayout(info, 1);
    root->addLayout(top);

    auto* timeLay = new QHBoxLayout;
    _timeL = new QLabel("0:00", this);
    _timeR = new QLabel("0:00", this);
    _timeL->setStyleSheet("font-size: 11px; color: #888;");
    _timeR->setStyleSheet("font-size: 11px; color: #888;");
    _progress = new QSlider(Qt::Horizontal, this);
    _progress->setRange(0, 1000);
    _progress->setStyleSheet(R"(
        QSlider::groove:horizontal {
            height: 4px; background: rgba(255,255,255,0.1); border-radius: 2px;
        }
        QSlider::handle:horizontal {
            width: 12px; height: 12px; margin: -4px 0;
            background: #88aaff; border-radius: 6px;
        }
        QSlider::sub-page:horizontal {
            background: #88aaff; border-radius: 2px;
        }
    )");
    connect(_progress, &QSlider::sliderPressed, this, [this]() { _seeking = true; });
    connect(_progress, &QSlider::sliderReleased, this, [this]() {
        _seeking = false;
        onSeek(_progress->value());
    });
    timeLay->addWidget(_timeL);
    timeLay->addWidget(_progress, 1);
    timeLay->addWidget(_timeR);
    root->addLayout(timeLay);

    auto* ctrl = new QHBoxLayout;
    ctrl->setSpacing(12);
    ctrl->addStretch();
    _btnPrev = new QPushButton("\xE2\x8F\xAE", this);
    _btnPlay = new QPushButton("\xE2\x96\xB6", this);
    _btnNext = new QPushButton("\xE2\x8F\xAD", this);
    for (auto* b : {_btnPrev, _btnNext}) {
        b->setFixedSize(40, 40);
        b->setStyleSheet("font-size: 18px; border-radius: 20px;");
    }
    _btnPlay->setFixedSize(52, 52);
    _btnPlay->setStyleSheet("font-size: 22px; border-radius: 26px; background: rgba(136,170,255,0.2);");
    connect(_btnPrev, &QPushButton::clicked, this, &MediaPlayer::onPrev);
    connect(_btnPlay, &QPushButton::clicked, this, &MediaPlayer::onPlayPause);
    connect(_btnNext, &QPushButton::clicked, this, &MediaPlayer::onNext);
    ctrl->addWidget(_btnPrev);
    ctrl->addWidget(_btnPlay);
    ctrl->addWidget(_btnNext);
    ctrl->addStretch();
    root->addLayout(ctrl);

    _noMedia = new QLabel("\xD0\x9D\xD0\xB8\xD1\x87\xD0\xB5\xD0\xB3\xD0\xBE \xD0\xBD\xD0\xB5 \xD0\xB8\xD0\xB3\xD1\x80\xD0\xB0\xD0\xB5\xD1\x82", this);
    _noMedia->setAlignment(Qt::AlignCenter);
    _noMedia->setStyleSheet("font-size: 15px; color: #666; padding: 40px;");
    root->addWidget(_noMedia);
    _noMedia->hide();

    root->addStretch();

    DBusError err;
    dbus_error_init(&err);
    _dbus = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        dbus_error_free(&err);
        _dbus = nullptr;
    }
    if (_dbus) dbus_connection_set_exit_on_disconnect(_dbus, FALSE);

    _timer = new QTimer(this);
    connect(_timer, &QTimer::timeout, this, &MediaPlayer::pollMpris);
    _timer->start(2000);
    pollMpris();
}

MediaPlayer::~MediaPlayer() {
    if (_dbus) { dbus_connection_unref(_dbus); _dbus = nullptr; }
}

bool MediaPlayer::fetchMpris(PanelMpris& out) {
    out = {};
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
            std::string name = dbus_str(&sub);
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

    DBusMessageIter pi;
    dbus_message_iter_init(reply, &pi);
    if (dbus_message_iter_get_arg_type(&pi) == DBUS_TYPE_ARRAY) {
        DBusMessageIter arr;
        dbus_message_iter_recurse(&pi, &arr);
        while (dbus_message_iter_get_arg_type(&arr) == DBUS_TYPE_DICT_ENTRY) {
            DBusMessageIter entry;
            dbus_message_iter_recurse(&arr, &entry);
            std::string key = dbus_str(&entry);
            dbus_message_iter_next(&entry);
            if (dbus_message_iter_get_arg_type(&entry) != DBUS_TYPE_VARIANT) {
                dbus_message_iter_next(&arr); continue;
            }
            DBusMessageIter var;
            dbus_message_iter_recurse(&entry, &var);

            if (key == "PlaybackStatus") {
                std::string st = dbus_str(&var);
                out.playing = (st == "Playing");
            } else if (key == "Position" &&
                       dbus_message_iter_get_arg_type(&var) == DBUS_TYPE_INT64) {
                dbus_message_iter_get_basic(&var, &out.position_us);
            } else if (key == "Metadata" &&
                       dbus_message_iter_get_arg_type(&var) == DBUS_TYPE_ARRAY) {
                DBusMessageIter meta;
                dbus_message_iter_recurse(&var, &meta);
                while (dbus_message_iter_get_arg_type(&meta) == DBUS_TYPE_DICT_ENTRY) {
                    DBusMessageIter me;
                    dbus_message_iter_recurse(&meta, &me);
                    std::string mk = dbus_str(&me);
                    dbus_message_iter_next(&me);
                    if (dbus_message_iter_get_arg_type(&me) != DBUS_TYPE_VARIANT) {
                        dbus_message_iter_next(&meta); continue;
                    }
                    DBusMessageIter mv;
                    dbus_message_iter_recurse(&me, &mv);
                    if (mk == "xesam:title" && dbus_message_iter_get_arg_type(&mv) == DBUS_TYPE_STRING)
                        out.title = dbus_str(&mv);
                    else if (mk == "xesam:artist" && dbus_message_iter_get_arg_type(&mv) == DBUS_TYPE_ARRAY) {
                        DBusMessageIter as;
                        dbus_message_iter_recurse(&mv, &as);
                        if (dbus_message_iter_get_arg_type(&as) == DBUS_TYPE_STRING)
                            out.artist = dbus_str(&as);
                    }
                    else if (mk == "mpris:artUrl" && dbus_message_iter_get_arg_type(&mv) == DBUS_TYPE_STRING)
                        out.art_url = dbus_str(&mv);
                    else if (mk == "mpris:length" && dbus_message_iter_get_arg_type(&mv) == DBUS_TYPE_INT64)
                        dbus_message_iter_get_basic(&mv, &out.length_us);
                    dbus_message_iter_next(&meta);
                }
            }
            dbus_message_iter_next(&arr);
        }
    }
    dbus_message_unref(reply);
    out.valid = true;
    out.bus = bus;
    return true;
}

void MediaPlayer::pollMpris() {
    PanelMpris info;
    if (fetchMpris(info)) {
        _info = info;
        updateUi();
    } else {
        _info = {};
        _title->hide(); _artist->hide(); _art->hide();
        _progress->hide(); _timeL->hide(); _timeR->hide();
        _btnPrev->hide(); _btnPlay->hide(); _btnNext->hide();
        _noMedia->show();
    }
}

void MediaPlayer::updateUi() {
    _noMedia->hide();
    _title->show(); _artist->show(); _art->show();
    _progress->show(); _timeL->show(); _timeR->show();
    _btnPrev->show(); _btnPlay->show(); _btnNext->show();

    _title->setText(QString::fromStdString(_info.title));
    _artist->setText(QString::fromStdString(_info.artist));
    _btnPlay->setText(_info.playing ? "\xE2\x8F\xB8" : "\xE2\x96\xB6");

    _timeL->setText(fmt_us(_info.position_us));
    _timeR->setText(fmt_us(_info.length_us));

    if (!_seeking && _info.length_us > 0) {
        int val = (int)((double)_info.position_us / _info.length_us * 1000);
        _progress->setValue(val);
    }

    std::string url = _info.art_url;
    if (url.rfind("file://", 0) == 0) url = url.substr(7);
    if (!url.empty() && QFile::exists(QString::fromStdString(url))) {
        QPixmap pm(QString::fromStdString(url));
        if (!pm.isNull())
            _art->setPixmap(pm.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void MediaPlayer::mprisCmd(const char* method) {
    if (!_dbus || _info.bus.empty()) return;
    DBusMessage* m = dbus_message_new_method_call(
        _info.bus.c_str(), "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player", method);
    if (!m) return;
    dbus_connection_send(_dbus, m, nullptr);
    dbus_message_unref(m);
    dbus_connection_flush(_dbus);
}

void MediaPlayer::onPlayPause() { mprisCmd("PlayPause"); _info.playing = !_info.playing; updateUi(); }
void MediaPlayer::onNext()      { mprisCmd("Next"); QTimer::singleShot(300, this, &MediaPlayer::pollMpris); }
void MediaPlayer::onPrev()      { mprisCmd("Previous"); QTimer::singleShot(300, this, &MediaPlayer::pollMpris); }

void MediaPlayer::onSeek(int val) {
    if (!_dbus || _info.bus.empty() || _info.length_us <= 0) return;
    int64_t pos = (int64_t)((double)val / 1000.0 * _info.length_us);
    DBusMessage* m = dbus_message_new_method_call(
        _info.bus.c_str(), "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player", "SetPosition");
    if (!m) return;
    const char* path = "/";
    dbus_message_append_args(m, DBUS_TYPE_OBJECT_PATH, &path,
                             DBUS_TYPE_INT64, &pos, DBUS_TYPE_INVALID);
    dbus_connection_send(_dbus, m, nullptr);
    dbus_message_unref(m);
    dbus_connection_flush(_dbus);
    _info.position_us = pos;
    updateUi();
}

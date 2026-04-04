#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <dbus/dbus.h>
#include <string>

struct PanelMpris {
    bool        valid       = false;
    bool        playing     = false;
    std::string title;
    std::string artist;
    std::string art_url;
    int64_t     length_us   = 0;
    int64_t     position_us = 0;
    std::string bus;
};

class MediaPlayer : public QWidget {
    Q_OBJECT
public:
    explicit MediaPlayer(QWidget* parent = nullptr);
    ~MediaPlayer() override;

private slots:
    void pollMpris();
    void onPlayPause();
    void onNext();
    void onPrev();
    void onSeek(int val);

private:
    void updateUi();
    bool fetchMpris(PanelMpris& out);
    void mprisCmd(const char* method);

    QLabel*      _art     = nullptr;
    QLabel*      _title   = nullptr;
    QLabel*      _artist  = nullptr;
    QLabel*      _timeL   = nullptr;
    QLabel*      _timeR   = nullptr;
    QSlider*     _progress= nullptr;
    QPushButton* _btnPrev = nullptr;
    QPushButton* _btnPlay = nullptr;
    QPushButton* _btnNext = nullptr;
    QLabel*      _noMedia = nullptr;

    QTimer*         _timer = nullptr;
    DBusConnection* _dbus  = nullptr;
    PanelMpris      _info;
    bool _seeking = false;
};

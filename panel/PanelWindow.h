#pragma once
#include <QWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QPropertyAnimation>

class Clock;
class MediaPlayer;
class FileBrowser;
class WallpaperPicker;
class AppManager;
class Terminal;

struct _XDisplay;

class PanelWindow : public QWidget {
    Q_OBJECT
public:
    explicit PanelWindow(QWidget* parent = nullptr);
    ~PanelWindow() override;

    void toggle();
    void reposition();

protected:
    void paintEvent(QPaintEvent* e) override;

private slots:
    void onTabClicked(int idx);

private:
    void setupUi();
    void applyStyle();
    void startXWatch();
    void pollX();

    QStackedWidget* _stack    = nullptr;
    QVBoxLayout*    _sidebar  = nullptr;
    QHBoxLayout*    _mainLay  = nullptr;
    QVector<QPushButton*> _tabs;

    Clock*           _clock     = nullptr;
    MediaPlayer*     _media     = nullptr;
    FileBrowser*     _files     = nullptr;
    WallpaperPicker* _wallpaper = nullptr;
    AppManager*      _apps      = nullptr;
    Terminal*        _terminal  = nullptr;

    QPropertyAnimation* _anim  = nullptr;
    bool _visible = false;
    int  _currentTab = 0;

    _XDisplay*      _xdpy  = nullptr;
    unsigned long   _toggle_atom = 0;
    QTimer*         _xpoll = nullptr;
    long            _toggle_counter = 0;

    static constexpr int PANEL_W = 420;
    static constexpr int PANEL_H = 520;
    static constexpr int SIDEBAR_W = 52;
    static constexpr int RADIUS = 16;
};

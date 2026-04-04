#include "PanelWindow.h"
#include "widgets/Clock.h"
#include "widgets/MediaPlayer.h"
#include "widgets/FileBrowser.h"
#include "widgets/WallpaperPicker.h"
#include "widgets/AppManager.h"
#include "widgets/Terminal.h"
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QGuiApplication>
#include <QIcon>

/* X11 включаем ПОСЛЕ Qt чтобы макросы не ломали Qt-заголовки */
#include <X11/Xlib.h>
#include <X11/Xatom.h>

PanelWindow::PanelWindow(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFixedSize(PANEL_W, PANEL_H);

    setupUi();
    applyStyle();
    reposition();
    startXWatch();

    _anim = new QPropertyAnimation(this, "windowOpacity", this);
    _anim->setDuration(150);
}

PanelWindow::~PanelWindow() {
    if (_xdpy) XCloseDisplay(_xdpy);
}

void PanelWindow::setupUi() {
    _mainLay = new QHBoxLayout(this);
    _mainLay->setContentsMargins(0, 0, 0, 0);
    _mainLay->setSpacing(0);

    auto* sideWidget = new QWidget(this);
    sideWidget->setFixedWidth(SIDEBAR_W);
    sideWidget->setObjectName("sidebar");
    _sidebar = new QVBoxLayout(sideWidget);
    _sidebar->setContentsMargins(4, 12, 4, 12);
    _sidebar->setSpacing(6);

    struct TabDef { QString icon; QString tip; };
    TabDef tabs[] = {
        {"\xF0\x9F\x8E\xB5", "Медиа"},
        {"\xF0\x9F\x93\x81", "Файлы"},
        {"\xF0\x9F\x93\xA6", "Приложения"},
        {"\xF0\x9F\x96\xBC", "Обои"},
    };

    for (int i = 0; i < 4; i++) {
        auto* btn = new QPushButton(tabs[i].icon, this);
        btn->setToolTip(tabs[i].tip);
        btn->setFixedSize(44, 44);
        btn->setCheckable(true);
        btn->setObjectName("tabBtn");
        btn->setProperty("tabIdx", i);
        connect(btn, &QPushButton::clicked, this, [this, i]() { onTabClicked(i); });
        _sidebar->addWidget(btn);
        _tabs.append(btn);
    }

    _sidebar->addStretch();
    _mainLay->addWidget(sideWidget);

    _stack = new QStackedWidget(this);
    _stack->setObjectName("content");

    _media     = new MediaPlayer(this);
    _files     = new FileBrowser(this);
    _apps      = new AppManager(this);
    _wallpaper = new WallpaperPicker(this);

    _stack->addWidget(_media);
    _stack->addWidget(_files);
    _stack->addWidget(_apps);
    _stack->addWidget(_wallpaper);

    _mainLay->addWidget(_stack);

    if (!_tabs.isEmpty()) _tabs[0]->setChecked(true);
}

void PanelWindow::applyStyle() {
    setStyleSheet(R"(
        QWidget {
            font-family: 'Noto Sans', 'DejaVu Sans', sans-serif;
            font-size: 13px;
            color: #e0e0e0;
        }
        QWidget#sidebar {
            background: transparent;
        }
        QPushButton#tabBtn {
            background: transparent;
            border: none;
            border-radius: 10px;
            font-size: 20px;
            padding: 6px;
        }
        QPushButton#tabBtn:hover {
            background: rgba(255,255,255,0.08);
        }
        QPushButton#tabBtn:checked {
            background: rgba(255,255,255,0.14);
        }
        QStackedWidget#content {
            background: transparent;
        }
        QScrollBar:vertical {
            width: 6px;
            background: transparent;
        }
        QScrollBar::handle:vertical {
            background: rgba(255,255,255,0.15);
            border-radius: 3px;
            min-height: 30px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QLineEdit {
            background: rgba(255,255,255,0.06);
            border: 1px solid rgba(255,255,255,0.1);
            border-radius: 8px;
            padding: 6px 10px;
            color: #e0e0e0;
        }
        QLineEdit:focus {
            border-color: rgba(136,170,255,0.5);
        }
        QPushButton {
            background: rgba(255,255,255,0.08);
            border: none;
            border-radius: 8px;
            padding: 8px 16px;
            color: #e0e0e0;
        }
        QPushButton:hover {
            background: rgba(255,255,255,0.14);
        }
        QPushButton:pressed {
            background: rgba(255,255,255,0.06);
        }
        QListWidget {
            background: transparent;
            border: none;
            outline: none;
        }
        QListWidget::item {
            padding: 8px 10px;
            border-radius: 8px;
        }
        QListWidget::item:hover {
            background: rgba(255,255,255,0.06);
        }
        QListWidget::item:selected {
            background: rgba(136,170,255,0.18);
        }
        QLabel {
            color: #e0e0e0;
        }
        QTabBar::tab {
            background: rgba(255,255,255,0.06);
            border: none;
            border-radius: 6px;
            padding: 6px 14px;
            margin-right: 4px;
            color: #aaa;
        }
        QTabBar::tab:selected {
            background: rgba(136,170,255,0.18);
            color: #fff;
        }
        QProgressBar {
            background: rgba(255,255,255,0.08);
            border: none;
            border-radius: 3px;
            height: 6px;
        }
        QProgressBar::chunk {
            background: #88aaff;
            border-radius: 3px;
        }
    )");
}

void PanelWindow::reposition() {
    auto* screen = QGuiApplication::primaryScreen();
    if (!screen) return;
    auto geo = screen->geometry();
    int x = geo.x() + (geo.width() - PANEL_W) / 2;
    int y = geo.y() + 40;
    move(x, y);
}

void PanelWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(rect(), RADIUS, RADIUS);
    p.setClipPath(path);
    p.fillRect(rect(), QColor(18, 18, 24, 230));
    p.setPen(QPen(QColor(255, 255, 255, 20), 1));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), RADIUS, RADIUS);
}

void PanelWindow::onTabClicked(int idx) {
    for (int i = 0; i < _tabs.size(); i++)
        _tabs[i]->setChecked(i == idx);
    _stack->setCurrentIndex(idx);
    _currentTab = idx;
}

void PanelWindow::toggle() {
    if (_visible) {
        _anim->setStartValue(1.0);
        _anim->setEndValue(0.0);
        _anim->start();
        connect(_anim, &QPropertyAnimation::finished, this, [this]() {
            hide();
            disconnect(_anim, &QPropertyAnimation::finished, nullptr, nullptr);
        });
        _visible = false;
    } else {
        reposition();
        setWindowOpacity(0.0);
        show();
        raise();
        _anim->setStartValue(0.0);
        _anim->setEndValue(1.0);
        _anim->start();
        _visible = true;
    }
}

void PanelWindow::startXWatch() {
    _xdpy = XOpenDisplay(nullptr);
    if (!_xdpy) return;

    _toggle_atom = XInternAtom(_xdpy, "_NEMAC_PANEL_TOGGLE", False);

    Window root = DefaultRootWindow(_xdpy);
    {
        Atom actual; int fmt; unsigned long n, after; unsigned char* data = nullptr;
        if (XGetWindowProperty(_xdpy, root, _toggle_atom, 0, 1, False,
                XA_CARDINAL, &actual, &fmt, &n, &after, &data) == Success && data) {
            _toggle_counter = *(long*)data;
            XFree(data);
        }
    }
    XSelectInput(_xdpy, root, PropertyChangeMask);

    _xpoll = new QTimer(this);
    connect(_xpoll, &QTimer::timeout, this, &PanelWindow::pollX);
    _xpoll->start(50);
}

void PanelWindow::pollX() {
    if (!_xdpy) return;
    while (XPending(_xdpy)) {
        XEvent ev;
        XNextEvent(_xdpy, &ev);
        if (ev.type == PropertyNotify &&
            (unsigned long)ev.xproperty.atom == _toggle_atom) {
            Window root = DefaultRootWindow(_xdpy);
            Atom actual; int fmt; unsigned long n, after; unsigned char* data = nullptr;
            if (XGetWindowProperty(_xdpy, root, (Atom)_toggle_atom, 0, 1, False,
                    XA_CARDINAL, &actual, &fmt, &n, &after, &data) == Success && data) {
                long val = *(long*)data;
                XFree(data);
                if (val != _toggle_counter) {
                    _toggle_counter = val;
                    toggle();
                }
            }
        }
    }
}

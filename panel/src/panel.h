#pragma once
#include <QQuickView>

typedef struct xcb_connection_t xcb_connection_t;
typedef uint32_t xcb_window_t;

class Panel : public QQuickView {
    Q_OBJECT

public:
    explicit Panel(QWindow* parent = nullptr);
    void load();
    Q_INVOKABLE void setPopupOpen(bool open);

private:
    void applyDockHints(xcb_connection_t* c, xcb_window_t w, bool withStrut);

    static constexpr int BAR_H = 36;
    static constexpr int POPUP_W = 340;
    static constexpr int POPUP_H = 152;

    int _screenX = 0;
    int _screenW = 0;
    QQuickView* _popup = nullptr;
};

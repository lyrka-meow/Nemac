#pragma once
#include <QQuickView>

class Panel : public QQuickView {
    Q_OBJECT

public:
    explicit Panel(QWindow* parent = nullptr);

private:
    void applyDockHints();

    static constexpr int HEIGHT = 36;
};

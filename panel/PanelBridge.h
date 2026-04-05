#pragma once
#include <QObject>
#include <QTimer>

struct _XDisplay;

class PanelBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool panelOpen READ panelOpen NOTIFY panelOpenChanged)
    Q_PROPERTY(int panelX READ panelX NOTIFY geometryChanged)
    Q_PROPERTY(int panelY READ panelY NOTIFY geometryChanged)
public:
    explicit PanelBridge(QObject* parent = nullptr);
    ~PanelBridge() override;

    bool panelOpen() const { return _open; }
    int  panelX() const { return _px; }
    int  panelY() const { return _py; }

public slots:
    void toggle();
    void reposition(int winW, int winH);

signals:
    void panelOpenChanged();
    void geometryChanged();
    void toggleRequested();

private:
    void initX();
    void pollX();

    _XDisplay*    _xdpy  = nullptr;
    unsigned long _atom   = 0;
    long          _counter = 0;
    QTimer*       _poll   = nullptr;
    bool          _open   = false;
    int           _px     = 0;
    int           _py     = 40;
};

#pragma once
#include <QWidget>
#include <QLabel>
#include <QTimer>

class Clock : public QWidget {
    Q_OBJECT
public:
    explicit Clock(QWidget* parent = nullptr);

private slots:
    void updateTime();

private:
    QLabel* _time = nullptr;
    QLabel* _date = nullptr;
    QTimer* _timer = nullptr;
};

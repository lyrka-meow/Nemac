#include "Clock.h"
#include <QVBoxLayout>
#include <QDateTime>
#include <QLocale>

Clock::Clock(QWidget* parent) : QWidget(parent) {
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 20, 16, 20);
    lay->setSpacing(4);

    _time = new QLabel(this);
    _time->setStyleSheet("font-size: 42px; font-weight: bold; color: #fff;");
    _time->setAlignment(Qt::AlignCenter);
    lay->addWidget(_time);

    _date = new QLabel(this);
    _date->setStyleSheet("font-size: 14px; color: #aaa;");
    _date->setAlignment(Qt::AlignCenter);
    lay->addWidget(_date);

    lay->addStretch();

    _timer = new QTimer(this);
    connect(_timer, &QTimer::timeout, this, &Clock::updateTime);
    _timer->start(1000);
    updateTime();
}

void Clock::updateTime() {
    auto now = QDateTime::currentDateTime();
    _time->setText(now.toString("HH:mm:ss"));
    _date->setText(now.toString("dddd, d MMMM yyyy"));
}

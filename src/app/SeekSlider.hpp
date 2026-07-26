#pragma once

#include <QList>
#include <QSlider>

#include "player/MpvVideoWidget.hpp"

// Seek bar that draws the cached lead behind the played fill, the way a
// streaming player does: dim track, lighter lead, bright played.
class SeekSlider final : public QSlider
{
    Q_OBJECT

public:
    explicit SeekSlider(QWidget *parent = nullptr);

    void setBuffered(const QList<MpvBufferedRange> &ranges);
    void setDuration(double seconds);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QList<MpvBufferedRange> m_buffered;
    double m_duration = 0.0;
};

#include "app/SeekSlider.hpp"

#include <QPainter>
#include <QProxyStyle>
#include <QStyleOptionSlider>

namespace {

// A click on the groove should jump to that spot rather than step one page.
// Setting the value absolutely also starts a drag, so the slider's existing
// sliderReleased handler performs the seek.
class AbsoluteSeekStyle final : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    int styleHint(
        StyleHint hint,
        const QStyleOption *option,
        const QWidget *widget,
        QStyleHintReturn *returnData
    ) const override
    {
        if (hint == SH_Slider_AbsoluteSetButtons) {
            return Qt::LeftButton;
        }
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

const QColor kTrackColor(0x55, 0x50, 0x4a);
const QColor kLeadColor(0x91, 0x8a, 0x80);

} // namespace

SeekSlider::SeekSlider(QWidget *parent)
    : QSlider(Qt::Horizontal, parent)
{
    setRange(0, 1000);
    auto *seekStyle = new AbsoluteSeekStyle;
    seekStyle->setParent(this);
    setStyle(seekStyle);
    // The track and the lead are painted here, so the stylesheet groove has to
    // stop drawing over them. Its height still comes from the app stylesheet,
    // and the played fill and handle are still drawn by the base class.
    setStyleSheet(QStringLiteral(
        "SeekSlider::groove:horizontal { background: transparent; }"
    ));
}

void SeekSlider::setBuffered(const QList<MpvBufferedRange> &ranges)
{
    if (m_buffered == ranges) {
        return;
    }
    m_buffered = ranges;
    update();
}

// The cache is reported in seconds, so the bar cannot place it until the
// duration is known, and the duration can arrive either side of the first
// cache report.
void SeekSlider::setDuration(double seconds)
{
    if (qFuzzyCompare(m_duration + 1.0, seconds + 1.0)) {
        return;
    }
    m_duration = seconds;
    update();
}

void SeekSlider::paintEvent(QPaintEvent *event)
{
    QStyleOptionSlider option;
    initStyleOption(&option);
    const QRect groove = style()->subControlRect(
        QStyle::CC_Slider,
        &option,
        QStyle::SC_SliderGroove,
        this
    );

    QPainter painter(this);
    painter.setPen(Qt::NoPen);
    painter.fillRect(groove, kTrackColor);

    if (m_duration > 0.0) {
        painter.setBrush(kLeadColor);
        const auto width = static_cast<double>(groove.width());
        for (const MpvBufferedRange &range : m_buffered) {
            const double from = qBound(0.0, range.start / m_duration, 1.0);
            const double to = qBound(from, range.end / m_duration, 1.0);
            painter.drawRect(QRectF(
                groove.x() + from * width,
                groove.y(),
                (to - from) * width,
                groove.height()
            ));
        }
    }
    painter.end();

    QSlider::paintEvent(event);
}

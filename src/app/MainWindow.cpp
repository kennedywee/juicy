#include "app/MainWindow.hpp"

#include <cmath>

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildInterface();
    setWindowTitle(QStringLiteral("Juicy"));
    resize(1100, 720);

    connect(m_player, &MpvVideoWidget::positionChanged, this, &MainWindow::updatePosition);
    connect(m_player, &MpvVideoWidget::durationChanged, this, &MainWindow::updateDuration);
    connect(m_player, &MpvVideoWidget::pauseChanged, this, &MainWindow::updatePaused);
    connect(m_player, &MpvVideoWidget::tracksChanged, this, &MainWindow::updateTracks);
    connect(
        m_player,
        &MpvVideoWidget::toggleFullscreenRequested,
        this,
        &MainWindow::toggleFullscreen
    );
}

MpvVideoWidget *MainWindow::player() const
{
    return m_player;
}

void MainWindow::openLocalFile(const QString &path)
{
    m_player->loadFile(path);
}

void MainWindow::chooseLocalFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Open video"),
        {},
        QStringLiteral("Video files (*.mkv *.mp4 *.webm *.avi *.mov);;All files (*)")
    );
    if (!path.isEmpty()) {
        openLocalFile(path);
    }
}

void MainWindow::chooseSubtitleFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Add subtitles"),
        {},
        QStringLiteral("Subtitle files (*.ass *.ssa *.srt *.vtt);;All files (*)")
    );
    if (!path.isEmpty()) {
        m_player->addSubtitleFile(path);
    }
}

void MainWindow::togglePlayback()
{
    m_player->setPaused(!m_paused);
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void MainWindow::updatePosition(double seconds)
{
    m_position = seconds;
    if (!m_seekSlider->isSliderDown() && m_duration > 0.0) {
        const double fraction = qBound(0.0, m_position / m_duration, 1.0);
        m_seekSlider->setValue(qRound(fraction * 1000.0));
    }
    updateTimeLabel();
}

void MainWindow::updateDuration(double seconds)
{
    m_duration = seconds;
    updateTimeLabel();
}

void MainWindow::updatePaused(bool paused)
{
    m_paused = paused;
    m_playButton->setIcon(style()->standardIcon(
        paused ? QStyle::SP_MediaPlay : QStyle::SP_MediaPause
    ));
    m_playButton->setToolTip(paused ? QStringLiteral("Play") : QStringLiteral("Pause"));
}

void MainWindow::updateTracks(const QList<MpvTrack> &tracks)
{
    const QSignalBlocker audioBlocker(m_audioTracks);
    const QSignalBlocker subtitleBlocker(m_subtitleTracks);
    m_audioTracks->clear();
    m_subtitleTracks->clear();
    m_subtitleTracks->addItem(QStringLiteral("Subtitles off"), QVariant::fromValue<qint64>(0));

    for (const MpvTrack &track : tracks) {
        QComboBox *combo = track.type == QStringLiteral("audio")
            ? m_audioTracks
            : m_subtitleTracks;
        combo->addItem(trackLabel(track), QVariant::fromValue(track.id));
        if (track.selected) {
            combo->setCurrentIndex(combo->count() - 1);
        }
    }

    m_audioTracks->setEnabled(m_audioTracks->count() > 0);
    m_subtitleTracks->setEnabled(m_subtitleTracks->count() > 1);
}

QString MainWindow::formatTime(double seconds)
{
    const qint64 totalSeconds = static_cast<qint64>(std::max(0.0, seconds));
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 remainder = totalSeconds % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(remainder, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(minutes)
        .arg(remainder, 2, 10, QLatin1Char('0'));
}

QString MainWindow::trackLabel(const MpvTrack &track)
{
    QStringList details;
    if (!track.title.isEmpty()) {
        details.push_back(track.title);
    }
    if (!track.language.isEmpty()) {
        details.push_back(track.language.toUpper());
    }
    if (track.external) {
        details.push_back(QStringLiteral("external"));
    }
    if (details.isEmpty()) {
        details.push_back(QStringLiteral("Track %1").arg(track.id));
    }
    return details.join(QStringLiteral(" · "));
}

void MainWindow::buildInterface()
{
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 8);
    layout->setSpacing(6);

    m_player = new MpvVideoWidget(container);
    m_player->setMinimumSize(640, 360);
    layout->addWidget(m_player, 1);

    auto *timeline = new QHBoxLayout;
    timeline->setContentsMargins(12, 0, 12, 0);
    m_seekSlider = new QSlider(Qt::Horizontal, container);
    m_seekSlider->setRange(0, 1000);
    m_timeLabel = new QLabel(QStringLiteral("0:00 / 0:00"), container);
    m_timeLabel->setMinimumWidth(105);
    timeline->addWidget(m_seekSlider, 1);
    timeline->addWidget(m_timeLabel);
    layout->addLayout(timeline);

    auto *controls = new QHBoxLayout;
    controls->setContentsMargins(12, 0, 12, 0);

    auto *openButton = new QPushButton(QStringLiteral("Open file"), container);
    m_playButton = new QPushButton(container);
    m_playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    m_playButton->setToolTip(QStringLiteral("Pause"));

    auto *volume = new QSlider(Qt::Horizontal, container);
    volume->setRange(0, 100);
    volume->setValue(100);
    volume->setMaximumWidth(110);
    volume->setToolTip(QStringLiteral("Volume"));

    m_audioTracks = new QComboBox(container);
    m_audioTracks->setMinimumWidth(135);
    m_audioTracks->setToolTip(QStringLiteral("Audio track"));
    m_audioTracks->setEnabled(false);

    m_subtitleTracks = new QComboBox(container);
    m_subtitleTracks->setMinimumWidth(155);
    m_subtitleTracks->setToolTip(QStringLiteral("Subtitle track"));
    m_subtitleTracks->setEnabled(false);

    auto *addSubtitle = new QPushButton(QStringLiteral("+ Subtitle"), container);
    m_anime4kProfile = new QComboBox(container);
    m_anime4kProfile->addItems({
        QStringLiteral("Anime4K off"),
        QStringLiteral("Anime4K fast"),
        QStringLiteral("Anime4K quality"),
    });
    m_anime4kProfile->setEnabled(false);

    auto *fullscreen = new QPushButton(container);
    fullscreen->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
    fullscreen->setToolTip(QStringLiteral("Fullscreen"));

    controls->addWidget(openButton);
    controls->addWidget(m_playButton);
    controls->addWidget(new QLabel(QStringLiteral("Volume"), container));
    controls->addWidget(volume);
    controls->addStretch(1);
    controls->addWidget(m_audioTracks);
    controls->addWidget(m_subtitleTracks);
    controls->addWidget(addSubtitle);
    controls->addWidget(m_anime4kProfile);
    controls->addWidget(fullscreen);
    layout->addLayout(controls);
    setCentralWidget(container);

    connect(openButton, &QPushButton::clicked, this, &MainWindow::chooseLocalFile);
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::togglePlayback);
    connect(addSubtitle, &QPushButton::clicked, this, &MainWindow::chooseSubtitleFile);
    connect(fullscreen, &QPushButton::clicked, this, &MainWindow::toggleFullscreen);
    connect(volume, &QSlider::valueChanged, m_player, &MpvVideoWidget::setVolume);
    connect(m_seekSlider, &QSlider::sliderReleased, this, [this] {
        if (m_duration > 0.0) {
            m_player->seekTo(
                m_duration * static_cast<double>(m_seekSlider->value()) / 1000.0
            );
        }
    });
    connect(m_audioTracks, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_player->selectAudioTrack(m_audioTracks->itemData(index).toLongLong());
    });
    connect(m_subtitleTracks, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_player->selectSubtitleTrack(m_subtitleTracks->itemData(index).toLongLong());
    });
}

void MainWindow::updateTimeLabel()
{
    m_timeLabel->setText(
        QStringLiteral("%1 / %2").arg(formatTime(m_position), formatTime(m_duration))
    );
}


#include "app/MainWindow.hpp"

#include <cmath>

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QGuiApplication>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProxyStyle>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

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

// Icons are painted rather than taken from a font: glyph fallback pulled each
// symbol from a different family, so they never shared a weight or size.
constexpr int kIconExtent = 14;

QPainter beginIcon(QPixmap &pixmap)
{
    const qreal ratio = QGuiApplication::primaryScreen() != nullptr
        ? QGuiApplication::primaryScreen()->devicePixelRatio()
        : 1.0;
    pixmap = QPixmap(QSize(kIconExtent, kIconExtent) * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    return QPainter(&pixmap);
}

QColor iconColor()
{
    return QGuiApplication::palette().color(QPalette::ButtonText);
}

QIcon playIcon()
{
    QPixmap pixmap;
    QPainter painter = beginIcon(pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(iconColor());
    painter.drawPolygon(QPolygonF({{3.5, 2.0}, {12.0, 7.0}, {3.5, 12.0}}));
    painter.end();
    return QIcon(pixmap);
}

QIcon pauseIcon()
{
    QPixmap pixmap;
    QPainter painter = beginIcon(pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(iconColor());
    painter.drawRect(QRectF(3.5, 2.0, 3.0, 10.0));
    painter.drawRect(QRectF(8.5, 2.0, 3.0, 10.0));
    painter.end();
    return QIcon(pixmap);
}

QIcon fullscreenIcon()
{
    QPixmap pixmap;
    QPainter painter = beginIcon(pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(iconColor(), 1.6, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    painter.setBrush(Qt::NoBrush);
    constexpr qreal near = 1.8;
    constexpr qreal far = kIconExtent - near;
    constexpr qreal arm = 4.0;
    QPainterPath corners;
    corners.moveTo(near, near + arm);
    corners.lineTo(near, near);
    corners.lineTo(near + arm, near);
    corners.moveTo(far - arm, near);
    corners.lineTo(far, near);
    corners.lineTo(far, near + arm);
    corners.moveTo(near, far - arm);
    corners.lineTo(near, far);
    corners.lineTo(near + arm, far);
    corners.moveTo(far - arm, far);
    corners.lineTo(far, far);
    corners.lineTo(far, far - arm);
    painter.drawPath(corners);
    painter.end();
    return QIcon(pixmap);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildInterface();
    m_torrentSession = new TorrentSession(this);
    setWindowTitle(QStringLiteral("Juicy"));
    resize(1100, 720);

    connect(m_player, &MpvVideoWidget::positionChanged, this, &MainWindow::updatePosition);
    connect(m_player, &MpvVideoWidget::durationChanged, this, &MainWindow::updateDuration);
    connect(m_player, &MpvVideoWidget::pauseChanged, this, &MainWindow::updatePaused);
    connect(m_player, &MpvVideoWidget::tracksChanged, this, &MainWindow::updateTracks);
    connect(m_player, &MpvVideoWidget::fatalError, this, [this](const QString &message) {
        m_statusBar->showMessage(message, 8000);
        m_playbackStatus->setText(QStringLiteral("Player error"));
    });
    connect(m_player, &MpvVideoWidget::playbackError, this, [this](const QString &message) {
        m_statusBar->showMessage(message, 8000);
        m_playbackStatus->setText(message);
    });
    connect(m_player, &MpvVideoWidget::playbackRetrying, this, [this](int attempt) {
        m_playbackStatus->setText(
            QStringLiteral("Retrying video… (%1/2)").arg(attempt)
        );
    });
    connect(m_player, &MpvVideoWidget::fileLoaded, this, [this] {
        m_torrentFileLoaded = true;
        m_playbackStatus->setText(QStringLiteral("Playing"));
    });
    connect(
        m_player,
        &MpvVideoWidget::toggleFullscreenRequested,
        this,
        &MainWindow::toggleFullscreen
    );
    connect(
        m_torrentSession,
        &TorrentSession::filesReady,
        this,
        &MainWindow::updateTorrentFiles
    );
    connect(m_torrentSession, &TorrentSession::statusChanged, this, [this](const QString &status) {
        m_statusBar->showMessage(status);
    });
    connect(m_torrentSession, &TorrentSession::errorOccurred, this, [this](const QString &message) {
        m_statusBar->showMessage(message, 8000);
    });
    connect(
        m_torrentSession,
        &TorrentSession::streamReady,
        this,
        [this](const std::shared_ptr<TorrentContent> &content, const TorrentFile &file) {
            m_torrentFileLoaded = false;
            m_player->setTorrentContent(content);
            m_player->loadFile(QStringLiteral("juicy://video"));
            m_playbackStatus->setText(QStringLiteral("Opening video…"));
            m_statusBar->showMessage(QStringLiteral("Buffering %1…").arg(file.name));
        }
    );
    connect(
        m_torrentSession,
        &TorrentSession::selectedFileComplete,
        this,
        [this](const QString &path) {
            if (!m_torrentFileLoaded) {
                m_playbackStatus->setText(QStringLiteral("Opening completed file…"));
                m_player->loadFile(path);
            }
        }
    );

    m_hideControlsTimer = new QTimer(this);
    m_hideControlsTimer->setSingleShot(true);
    m_hideControlsTimer->setInterval(3000);
    connect(m_hideControlsTimer, &QTimer::timeout, this, [this] {
        if (shouldKeepControlsVisible()) {
            m_hideControlsTimer->start();
        } else {
            setControlsVisible(false);
        }
    });
    m_clickTimer = new QTimer(this);
    m_clickTimer->setSingleShot(true);
    m_clickTimer->setInterval(QApplication::doubleClickInterval());
    connect(m_clickTimer, &QTimer::timeout, this, &MainWindow::togglePlayback);
    // The magnet field is first in tab order; give the video initial focus so
    // shortcuts work without clicking first.
    m_player->setFocus();
    m_player->setMouseTracking(true);
    m_player->installEventFilter(this);
    m_topPanel->installEventFilter(this);
    m_bottomPanel->installEventFilter(this);
    m_hideControlsTimer->start();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_player && event->type() == QEvent::MouseButtonPress) {
        const auto button = static_cast<QMouseEvent *>(event)->button();
        // Right-clicking the video dismisses the overlay straight away.
        if (button == Qt::RightButton) {
            m_hideControlsTimer->stop();
            setControlsVisible(false);
            return true;
        }
        // Hold the play/pause toggle until this is known not to be the first
        // half of a double click, which toggles fullscreen instead.
        if (button == Qt::LeftButton) {
            m_clickTimer->start();
        }
    }
    if (watched == m_player && event->type() == QEvent::MouseButtonDblClick) {
        m_clickTimer->stop();
    }
    if (event->type() == QEvent::MouseMove || event->type() == QEvent::Enter) {
        showControls();
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Only keys the focused widget ignored reach here, so typing in the magnet
    // field is never intercepted.
    switch (event->key()) {
    case Qt::Key_Space:
    case Qt::Key_K:
        togglePlayback();
        break;
    case Qt::Key_Left:
        seekBy(-5.0);
        break;
    case Qt::Key_Right:
        seekBy(5.0);
        break;
    case Qt::Key_J:
        seekBy(-10.0);
        break;
    case Qt::Key_L:
        seekBy(10.0);
        break;
    case Qt::Key_Up:
        adjustVolume(5);
        break;
    case Qt::Key_Down:
        adjustVolume(-5);
        break;
    case Qt::Key_M:
        toggleMute();
        break;
    case Qt::Key_F:
        toggleFullscreen();
        break;
    case Qt::Key_Escape:
        if (!isFullScreen()) {
            QMainWindow::keyPressEvent(event);
            return;
        }
        showNormal();
        break;
    default:
        QMainWindow::keyPressEvent(event);
        return;
    }

    showControls();
    event->accept();
}

void MainWindow::seekBy(double seconds)
{
    if (m_duration <= 0.0) {
        return;
    }
    const double target = qBound(0.0, m_position + seconds, m_duration);
    m_player->seekTo(target);
    m_statusBar->showMessage(
        QStringLiteral("%1%2s · %3")
            .arg(seconds > 0.0 ? QStringLiteral("+") : QString())
            .arg(seconds, 0, 'f', 0)
            .arg(formatTime(target)),
        2000
    );
}

void MainWindow::adjustVolume(int delta)
{
    // Drives the existing slider connection, so the UI stays in sync.
    const int volume = qBound(0, m_volumeSlider->value() + delta, 100);
    m_volumeSlider->setValue(volume);
    m_statusBar->showMessage(QStringLiteral("Volume %1%").arg(volume), 2000);
}

void MainWindow::toggleMute()
{
    m_muted = !m_muted;
    m_player->setMuted(m_muted);
    m_statusBar->showMessage(
        m_muted ? QStringLiteral("Muted") : QStringLiteral("Unmuted"),
        2000
    );
}

void MainWindow::showControls()
{
    setControlsVisible(true);
    m_hideControlsTimer->start();
}

void MainWindow::setControlsVisible(bool visible)
{
    m_topPanel->setVisible(visible);
    // The status bar is a child of the bottom panel and hides along with it.
    m_bottomPanel->setVisible(visible);
    m_player->setCursor(visible ? Qt::ArrowCursor : Qt::BlankCursor);
}

bool MainWindow::shouldKeepControlsVisible() const
{
    return !m_torrentFileLoaded
        || m_paused
        || QApplication::mouseButtons() != Qt::NoButton
        || m_magnetInput->hasFocus()
        || QApplication::activePopupWidget() != nullptr;
}

MpvVideoWidget *MainWindow::player() const
{
    return m_player;
}

void MainWindow::openLocalFile(const QString &path)
{
    m_player->loadFile(path);
}

void MainWindow::openMagnet(const QString &magnet)
{
    m_magnetInput->setText(magnet);
    loadMagnet();
}

void MainWindow::setAutoStream(bool enabled)
{
    m_autoStream = enabled;
}

void MainWindow::setAnime4kProfile(const QString &profile)
{
    const int index = m_anime4kProfile->findData(profile.toLower());
    if (index >= 0) {
        if (m_anime4kProfile->currentIndex() == index) {
            applyAnime4kProfile(index);
        } else {
            m_anime4kProfile->setCurrentIndex(index);
        }
    }
}

void MainWindow::setContentFit(const QString &mode)
{
    const int index = m_contentFit->findData(mode.toLower());
    if (index < 0) {
        return;
    }
    if (m_contentFit->currentIndex() == index) {
        m_player->setContentFit(mode.toLower());
    } else {
        m_contentFit->setCurrentIndex(index);
    }
}

void MainWindow::loadMagnet()
{
    // Hand focus to the video so shortcuts work straight after clicking Load.
    m_player->setFocus();
    m_videoFiles->clear();
    m_videoFiles->setEnabled(false);
    m_streamButton->setEnabled(false);
    m_torrentSession->addMagnet(m_magnetInput->text().trimmed());
}

void MainWindow::startTorrentPlayback()
{
    m_player->setFocus();
    const int fileIndex = m_videoFiles->currentData().toInt();
    m_torrentSession->selectFile(fileIndex);
}

void MainWindow::updateTorrentFiles(const QList<TorrentFile> &files)
{
    m_videoFiles->clear();
    for (const TorrentFile &file : files) {
        const double mebibytes = static_cast<double>(file.size) / (1024.0 * 1024.0);
        m_videoFiles->addItem(
            QStringLiteral("%1 · %2 MiB").arg(file.name).arg(mebibytes, 0, 'f', 1),
            file.index
        );
    }
    const bool hasFiles = !files.isEmpty();
    m_videoFiles->setEnabled(hasFiles);
    m_streamButton->setEnabled(hasFiles);
    if (hasFiles && m_autoStream) {
        startTorrentPlayback();
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
    if (paused) {
        showControls();
    }
    m_playButton->setIcon(paused ? m_playIcon : m_pauseIcon);
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

void MainWindow::applyAnime4kProfile(int index)
{
    const QStringList shaderFiles = anime4kShaderFiles(index);
    if (index > 0 && shaderFiles.isEmpty()) {
        m_statusBar->showMessage(QStringLiteral("Anime4K shaders are unavailable."), 5000);
        m_anime4kProfile->setCurrentIndex(0);
        m_player->setShaderFiles({});
        return;
    }

    m_player->setShaderFiles(shaderFiles);
    const QString label = index > 0
        ? m_anime4kProfile->itemText(index)
        : QStringLiteral("Anime4K disabled");
    m_statusBar->showMessage(label, 2500);
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

QString MainWindow::anime4kDirectory()
{
    const QString installed = QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("../share/juicy/anime4k"));
    if (QFileInfo::exists(QDir(installed).filePath(QStringLiteral("LICENSE")))) {
        return QDir::cleanPath(installed);
    }

    const QString source = QStringLiteral(JUICY_SOURCE_ANIME4K_DIR);
    if (QFileInfo::exists(QDir(source).filePath(QStringLiteral("LICENSE")))) {
        return source;
    }
    return {};
}

QStringList MainWindow::anime4kShaderFiles(int profileIndex)
{
    if (profileIndex <= 0) {
        return {};
    }

    const QString directory = anime4kDirectory();
    if (directory.isEmpty()) {
        return {};
    }

    QStringList names;
    if (profileIndex == 1) {
        names = {
            QStringLiteral("Anime4K_Clamp_Highlights.glsl"),
            QStringLiteral("Anime4K_Restore_CNN_S.glsl"),
            QStringLiteral("Anime4K_Upscale_CNN_x2_S.glsl"),
            QStringLiteral("Anime4K_Upscale_CNN_x2_S.glsl"),
        };
    } else {
        names = {
            QStringLiteral("Anime4K_Clamp_Highlights.glsl"),
            QStringLiteral("Anime4K_Restore_CNN_L.glsl"),
            QStringLiteral("Anime4K_Upscale_CNN_x2_L.glsl"),
            QStringLiteral("Anime4K_Restore_CNN_S.glsl"),
            QStringLiteral("Anime4K_Upscale_CNN_x2_S.glsl"),
        };
    }

    QStringList paths;
    paths.reserve(names.size());
    const QDir shaderDirectory(directory);
    for (const QString &name : names) {
        const QString path = shaderDirectory.filePath(name);
        if (!QFileInfo::exists(path)) {
            return {};
        }
        paths.push_back(path);
    }
    return paths;
}

void MainWindow::buildInterface()
{
    auto *container = new QWidget(this);
    auto *layout = new QGridLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_player = new MpvVideoWidget(container);
    m_player->setMinimumSize(640, 360);
    layout->addWidget(m_player, 0, 0);

    m_topPanel = new QWidget(container);
    m_topPanel->setObjectName(QStringLiteral("overlayPanel"));
    m_topPanel->setAttribute(Qt::WA_StyledBackground, true);
    auto *sourceBar = new QHBoxLayout(m_topPanel);
    sourceBar->setContentsMargins(12, 8, 12, 8);
    m_magnetInput = new QLineEdit(container);
    m_magnetInput->setPlaceholderText(QStringLiteral("Paste a magnet link"));
    m_magnetInput->setClearButtonEnabled(true);
    auto *loadButton = new QPushButton(QStringLiteral("Load"), container);
    m_videoFiles = new QComboBox(container);
    m_videoFiles->setMinimumWidth(260);
    m_videoFiles->setEnabled(false);
    m_streamButton = new QPushButton(QStringLiteral("Stream"), container);
    m_streamButton->setEnabled(false);
    sourceBar->addWidget(m_magnetInput, 1);
    sourceBar->addWidget(loadButton);
    sourceBar->addWidget(m_videoFiles);
    sourceBar->addWidget(m_streamButton);
    layout->addWidget(m_topPanel, 0, 0, Qt::AlignTop);

    m_bottomPanel = new QWidget(container);
    m_bottomPanel->setObjectName(QStringLiteral("overlayPanel"));
    m_bottomPanel->setAttribute(Qt::WA_StyledBackground, true);
    auto *bottomLayout = new QVBoxLayout(m_bottomPanel);
    bottomLayout->setContentsMargins(0, 6, 0, 0);
    bottomLayout->setSpacing(6);

    auto *timeline = new QHBoxLayout;
    timeline->setContentsMargins(12, 0, 12, 0);
    m_seekSlider = new QSlider(Qt::Horizontal, container);
    m_seekSlider->setRange(0, 1000);
    auto *seekStyle = new AbsoluteSeekStyle;
    seekStyle->setParent(m_seekSlider);
    m_seekSlider->setStyle(seekStyle);
    m_timeLabel = new QLabel(QStringLiteral("0:00 / 0:00"), container);
    m_timeLabel->setMinimumWidth(105);
    timeline->addWidget(m_seekSlider, 1);
    timeline->addWidget(m_timeLabel);
    bottomLayout->addLayout(timeline);

    auto *controls = new QHBoxLayout;
    controls->setContentsMargins(12, 0, 12, 0);

    m_playIcon = playIcon();
    m_pauseIcon = pauseIcon();
    m_playButton = new QPushButton(container);
    m_playButton->setIcon(m_pauseIcon);
    m_playButton->setFixedWidth(46);
    m_playButton->setToolTip(QStringLiteral("Pause"));

    m_volumeSlider = new QSlider(Qt::Horizontal, container);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(100);
    m_volumeSlider->setMaximumWidth(110);
    m_volumeSlider->setToolTip(QStringLiteral("Volume"));

    m_audioTracks = new QComboBox(container);
    m_audioTracks->setMinimumWidth(135);
    m_audioTracks->setToolTip(QStringLiteral("Audio track"));
    m_audioTracks->setEnabled(false);

    m_subtitleTracks = new QComboBox(container);
    m_subtitleTracks->setMinimumWidth(155);
    m_subtitleTracks->setToolTip(QStringLiteral("Subtitle track"));
    m_subtitleTracks->setEnabled(false);

    m_contentFit = new QComboBox(container);
    m_contentFit->setToolTip(QStringLiteral("Content fit"));
    m_contentFit->addItem(QStringLiteral("Fit"), QStringLiteral("fit"));
    m_contentFit->addItem(QStringLiteral("Cover"), QStringLiteral("cover"));
    m_contentFit->addItem(QStringLiteral("Stretch"), QStringLiteral("stretch"));
    m_contentFit->addItem(QStringLiteral("Original"), QStringLiteral("original"));

    auto *addSubtitle = new QPushButton(QStringLiteral("+ Subtitle"), container);
    m_anime4kProfile = new QComboBox(container);
    m_anime4kProfile->addItem(QStringLiteral("Anime4K off"), QStringLiteral("off"));
    m_anime4kProfile->addItem(QStringLiteral("Anime4K fast"), QStringLiteral("fast"));
    m_anime4kProfile->addItem(QStringLiteral("Anime4K quality"), QStringLiteral("quality"));
    m_anime4kProfile->setEnabled(!anime4kDirectory().isEmpty());

    auto *fullscreen = new QPushButton(container);
    fullscreen->setIcon(fullscreenIcon());
    fullscreen->setFixedWidth(46);
    fullscreen->setToolTip(QStringLiteral("Fullscreen"));

    controls->addWidget(m_playButton);
    controls->addWidget(new QLabel(QStringLiteral("Volume"), container));
    controls->addWidget(m_volumeSlider);
    controls->addStretch(1);
    controls->addWidget(m_audioTracks);
    controls->addWidget(m_subtitleTracks);
    controls->addWidget(addSubtitle);
    controls->addWidget(m_contentFit);
    controls->addWidget(m_anime4kProfile);
    controls->addWidget(fullscreen);
    bottomLayout->addLayout(controls);

    // Lives inside the overlay rather than QMainWindow's own status bar, which
    // sat below the central widget and resized the video whenever it hid.
    m_statusBar = new QStatusBar(m_bottomPanel);
    m_statusBar->setObjectName(QStringLiteral("overlayStatus"));
    m_statusBar->setSizeGripEnabled(false);
    bottomLayout->addWidget(m_statusBar);
    layout->addWidget(m_bottomPanel, 0, 0, Qt::AlignBottom);

    const QString overlayStyle = QStringLiteral(
        "#overlayPanel { background-color: rgba(40, 35, 32, 215); }"
        "#overlayStatus { background-color: #282320; }"
    );
    m_topPanel->setStyleSheet(overlayStyle);
    m_bottomPanel->setStyleSheet(overlayStyle);
    m_topPanel->raise();
    m_bottomPanel->raise();
    setCentralWidget(container);
    m_playbackStatus = new QLabel(QStringLiteral("Idle"), m_statusBar);
    m_statusBar->addPermanentWidget(m_playbackStatus);

    connect(loadButton, &QPushButton::clicked, this, &MainWindow::loadMagnet);
    connect(m_magnetInput, &QLineEdit::returnPressed, this, &MainWindow::loadMagnet);
    connect(m_streamButton, &QPushButton::clicked, this, &MainWindow::startTorrentPlayback);
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::togglePlayback);
    connect(addSubtitle, &QPushButton::clicked, this, &MainWindow::chooseSubtitleFile);
    connect(fullscreen, &QPushButton::clicked, this, &MainWindow::toggleFullscreen);
    connect(m_volumeSlider, &QSlider::valueChanged, m_player, &MpvVideoWidget::setVolume);
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
    connect(m_contentFit, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_player->setContentFit(m_contentFit->itemData(index).toString());
        m_statusBar->showMessage(
            QStringLiteral("Content fit: %1").arg(m_contentFit->itemText(index)),
            2500
        );
    });
    connect(
        m_anime4kProfile,
        &QComboBox::currentIndexChanged,
        this,
        &MainWindow::applyAnime4kProfile
    );
}

void MainWindow::updateTimeLabel()
{
    m_timeLabel->setText(
        QStringLiteral("%1 / %2").arg(formatTime(m_position), formatTime(m_duration))
    );
}

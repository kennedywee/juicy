#include "app/MainWindow.hpp"

#include <cmath>

#include <QAction>
#include <QActionGroup>
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
#include <QMenu>
#include <QMouseEvent>
#include <QPair>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProxyStyle>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QTimer>
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

// Both overlay bars are 8px padding around a ~26px control. The floating
// diagnostics panel and toast inset by this much so they never sit under one.
constexpr int kBarHeight = 44;

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

void drawSpeakerBody(QPainter &painter)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(iconColor());
    painter.drawPolygon(QPolygonF({
        {1.5, 5.0}, {4.0, 5.0}, {7.0, 2.0},
        {7.0, 12.0}, {4.0, 9.0}, {1.5, 9.0},
    }));
}

QIcon volumeIcon()
{
    QPixmap pixmap;
    QPainter painter = beginIcon(pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    drawSpeakerBody(painter);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(iconColor(), 1.3, Qt::SolidLine, Qt::RoundCap));
    // Sound waves: arcs opening to the right of the cone.
    painter.drawArc(QRectF(6.0, 4.0, 4.0, 6.0), -80 * 16, 160 * 16);
    painter.drawArc(QRectF(6.0, 1.5, 7.0, 11.0), -70 * 16, 140 * 16);
    painter.end();
    return QIcon(pixmap);
}

QIcon mutedIcon()
{
    QPixmap pixmap;
    QPainter painter = beginIcon(pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    drawSpeakerBody(painter);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(iconColor(), 1.4, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(9.0, 5.0), QPointF(12.5, 9.0));
    painter.drawLine(QPointF(12.5, 5.0), QPointF(9.0, 9.0));
    painter.end();
    return QIcon(pixmap);
}

QIcon settingsIcon()
{
    QPixmap pixmap;
    QPainter painter = beginIcon(pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(kIconExtent / 2.0, kIconExtent / 2.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(iconColor());
    // Solid cog with the hub punched back out, so it matches the filled
    // play/pause icons instead of reading as a thin sunburst.
    painter.drawEllipse(QPointF(0.0, 0.0), 4.4, 4.4);
    for (int tooth = 0; tooth < 8; ++tooth) {
        painter.drawRoundedRect(QRectF(-1.15, -6.3, 2.3, 3.0), 0.6, 0.6);
        painter.rotate(45.0);
    }
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.drawEllipse(QPointF(0.0, 0.0), 1.7, 1.7);
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
    m_diagnostics->setPlaybackState(QStringLiteral("Idle"));
    m_torrentSession = new TorrentSession(this);
    setWindowTitle(QStringLiteral("Juicy"));
    resize(1100, 720);

    connect(m_player, &MpvVideoWidget::positionChanged, this, &MainWindow::updatePosition);
    connect(m_player, &MpvVideoWidget::durationChanged, this, &MainWindow::updateDuration);
    connect(m_player, &MpvVideoWidget::pauseChanged, this, &MainWindow::updatePaused);
    connect(m_player, &MpvVideoWidget::tracksChanged, this, &MainWindow::updateTracks);
    connect(m_player, &MpvVideoWidget::fatalError, this, [this](const QString &message) {
        showToast(message);
        m_diagnostics->setError(message);
        m_diagnostics->setPlaybackState(QStringLiteral("Player error"));
    });
    connect(m_player, &MpvVideoWidget::playbackError, this, [this](const QString &message) {
        showToast(message);
        m_diagnostics->setError(message);
        m_diagnostics->setPlaybackState(QStringLiteral("Error"));
    });
    connect(m_player, &MpvVideoWidget::playbackRetrying, this, [this](int attempt) {
        const QString message = QStringLiteral("Retrying video… (%1/2)").arg(attempt);
        showToast(message);
        m_diagnostics->setPlaybackState(message);
    });
    connect(m_player, &MpvVideoWidget::fileLoaded, this, [this] {
        m_torrentFileLoaded = true;
        m_diagnostics->setPlaybackState(QStringLiteral("Playing"));
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
    connect(
        m_torrentSession,
        &TorrentSession::statusChanged,
        m_diagnostics,
        &DiagnosticsPanel::setTorrentStatus
    );
    connect(
        m_torrentSession,
        &TorrentSession::statsChanged,
        m_diagnostics,
        &DiagnosticsPanel::setTorrentStats
    );
    connect(m_torrentSession, &TorrentSession::errorOccurred, this, [this](const QString &message) {
        showToast(message);
        m_diagnostics->setError(message);
    });
    connect(
        m_torrentSession,
        &TorrentSession::streamReady,
        this,
        [this](const std::shared_ptr<TorrentContent> &content, const TorrentFile &file) {
            m_torrentFileLoaded = false;
            m_player->setTorrentContent(content);
            m_player->loadFile(QStringLiteral("juicy://video"));
            m_diagnostics->setPlaybackState(QStringLiteral("Opening video…"));
            showToast(QStringLiteral("Buffering %1…").arg(file.name));
        }
    );
    connect(
        m_torrentSession,
        &TorrentSession::selectedFileComplete,
        this,
        [this](const QString &path) {
            if (!m_torrentFileLoaded) {
                m_diagnostics->setPlaybackState(QStringLiteral("Opening completed file…"));
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
    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    m_toastTimer->setInterval(6000);
    connect(m_toastTimer, &QTimer::timeout, m_toast, &QLabel::hide);
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
    case Qt::Key_D:
        setDiagnosticsVisible(!m_diagnostics->isVisible());
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
}

void MainWindow::adjustVolume(int delta)
{
    // Drives the existing slider connection, so the UI stays in sync.
    m_volumeSlider->setValue(qBound(0, m_volumeSlider->value() + delta, 100));
}

void MainWindow::toggleMute()
{
    m_muted = !m_muted;
    m_player->setMuted(m_muted);
    m_volumeButton->setIcon(m_muted ? m_mutedIcon : m_volumeIcon);
    m_volumeButton->setToolTip(
        m_muted ? QStringLiteral("Unmute") : QStringLiteral("Mute")
    );
}

// Errors and buffering only: routine volume/seek/fit changes are already
// visible in the bar, and any key press brings the bar back on screen.
void MainWindow::showToast(const QString &message)
{
    m_toast->setText(message);
    m_toast->show();
    m_toast->raise();
    m_toastTimer->start();
}

void MainWindow::setDiagnosticsVisible(bool visible)
{
    m_diagnostics->setVisible(visible);
    m_diagnostics->raise();
    const QSignalBlocker blocker(m_diagnosticsAction);
    m_diagnosticsAction->setChecked(visible);
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
    const QList<QAction *> actions = m_anime4kMenu->actions();
    for (int index = 0; index < actions.size(); ++index) {
        if (actions.at(index)->data().toString() == profile.toLower()) {
            actions.at(index)->setChecked(true);
            applyAnime4kProfile(index);
            return;
        }
    }
}

void MainWindow::setContentFit(const QString &mode)
{
    for (QAction *action : m_contentFitMenu->actions()) {
        if (action->data().toString() == mode.toLower()) {
            action->setChecked(true);
            m_player->setContentFit(mode.toLower());
            return;
        }
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
    if (m_torrentFileLoaded) {
        m_diagnostics->setPlaybackState(
            paused ? QStringLiteral("Paused") : QStringLiteral("Playing")
        );
    }
}

void MainWindow::updateTracks(const QList<MpvTrack> &tracks)
{
    populateTrackMenu(
        m_audioMenu,
        m_audioGroup,
        tracks,
        QStringLiteral("audio"),
        false,
        &MpvVideoWidget::selectAudioTrack
    );
    populateTrackMenu(
        m_subtitleMenu,
        m_subtitleGroup,
        tracks,
        QStringLiteral("sub"),
        true,
        &MpvVideoWidget::selectSubtitleTrack
    );
}

void MainWindow::populateTrackMenu(
    QMenu *menu,
    QActionGroup *&group,
    const QList<MpvTrack> &tracks,
    const QString &type,
    bool includeOff,
    void (MpvVideoWidget::*select)(qint64)
)
{
    menu->clear();
    // The group owns nothing the menu just deleted, but it outlives clear(),
    // so it has to go too or stale groups pile up on every track change.
    delete group;
    group = new QActionGroup(menu);
    group->setExclusive(true);

    const auto addEntry = [&](const QString &text, qint64 id, bool checked) {
        QAction *action = menu->addAction(text);
        action->setCheckable(true);
        action->setChecked(checked);
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, select, id] {
            (m_player->*select)(id);
        });
    };

    bool anySelected = false;
    for (const MpvTrack &track : tracks) {
        if (track.type != type) {
            continue;
        }
        anySelected = anySelected || track.selected;
    }
    if (includeOff) {
        addEntry(QStringLiteral("Off"), 0, !anySelected);
    }
    for (const MpvTrack &track : tracks) {
        if (track.type == type) {
            addEntry(trackLabel(track), track.id, track.selected);
        }
    }
    menu->setEnabled(menu->actions().size() > (includeOff ? 1 : 0));
}

void MainWindow::applyAnime4kProfile(int index)
{
    const QStringList shaderFiles = anime4kShaderFiles(index);
    if (index > 0 && shaderFiles.isEmpty()) {
        showToast(QStringLiteral("Anime4K shaders are unavailable."));
        m_anime4kMenu->actions().constFirst()->setChecked(true);
        m_player->setShaderFiles({});
        return;
    }
    m_player->setShaderFiles(shaderFiles);
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

    // Transparent layer between the video and the control panels, so the
    // diagnostics panel and the toast can float without hiding with the bar.
    // Mouse-transparent: clicks fall through to the video underneath.
    auto *overlayLayer = new QWidget(container);
    overlayLayer->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    auto *overlayGrid = new QGridLayout(overlayLayer);
    overlayGrid->setContentsMargins(12, kBarHeight + 12, 12, kBarHeight + 12);

    m_diagnostics = new DiagnosticsPanel(overlayLayer);
    m_diagnostics->hide();
    connect(m_diagnostics, &DiagnosticsPanel::closeRequested, this, [this] {
        setDiagnosticsVisible(false);
    });
    overlayGrid->addWidget(m_diagnostics, 0, 0, Qt::AlignTop | Qt::AlignRight);

    m_toast = new QLabel(overlayLayer);
    m_toast->setObjectName(QStringLiteral("toast"));
    m_toast->setAttribute(Qt::WA_StyledBackground, true);
    m_toast->hide();
    // Top-left, not bottom-left: the bottom of the frame is the subtitle band,
    // which is exactly what this redesign set out to keep clear.
    overlayGrid->addWidget(m_toast, 0, 0, Qt::AlignTop | Qt::AlignLeft);
    layout->addWidget(overlayLayer, 0, 0);

    m_bottomPanel = new QWidget(container);
    m_bottomPanel->setObjectName(QStringLiteral("overlayPanel"));
    m_bottomPanel->setAttribute(Qt::WA_StyledBackground, true);
    // One row, same padding as the source bar above, so both read as one frame.
    auto *controls = new QHBoxLayout(m_bottomPanel);
    controls->setContentsMargins(12, 8, 12, 8);
    controls->setSpacing(8);

    m_playIcon = playIcon();
    m_pauseIcon = pauseIcon();
    m_volumeIcon = volumeIcon();
    m_mutedIcon = mutedIcon();

    const auto barButton = [&container](const QIcon &icon, const QString &tip) {
        auto *button = new QPushButton(container);
        button->setObjectName(QStringLiteral("barButton"));
        button->setIcon(icon);
        button->setFixedWidth(34);
        button->setToolTip(tip);
        return button;
    };

    m_playButton = barButton(m_pauseIcon, QStringLiteral("Pause"));
    m_volumeButton = barButton(m_volumeIcon, QStringLiteral("Mute"));
    auto *settingsButton = barButton(settingsIcon(), QStringLiteral("Settings"));
    auto *fullscreen = barButton(fullscreenIcon(), QStringLiteral("Fullscreen"));

    m_seekSlider = new QSlider(Qt::Horizontal, container);
    m_seekSlider->setRange(0, 1000);
    auto *seekStyle = new AbsoluteSeekStyle;
    seekStyle->setParent(m_seekSlider);
    m_seekSlider->setStyle(seekStyle);

    m_timeLabel = new QLabel(QStringLiteral("0:00 / 0:00"), container);
    m_timeLabel->setAlignment(Qt::AlignCenter);

    m_volumeSlider = new QSlider(Qt::Horizontal, container);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(100);
    m_volumeSlider->setFixedWidth(84);
    m_volumeSlider->setToolTip(QStringLiteral("Volume"));

    controls->addWidget(m_playButton);
    controls->addWidget(m_seekSlider, 1);
    controls->addWidget(m_timeLabel);
    controls->addWidget(m_volumeButton);
    controls->addWidget(m_volumeSlider);
    controls->addWidget(settingsButton);
    controls->addWidget(fullscreen);
    // A line edit is a few pixels taller than a push button, so the two bars
    // would not match on their own. Pin the control bar to the source bar.
    m_bottomPanel->setFixedHeight(m_topPanel->sizeHint().height());
    layout->addWidget(m_bottomPanel, 0, 0, Qt::AlignBottom);

    buildSettingsMenu();

    const QString overlayStyle = QStringLiteral(
        "#overlayPanel { background-color: rgba(40, 35, 32, 215); }"
        "#barButton { padding: 4px 8px; }"
        "#toast {"
        "  background-color: rgba(40, 35, 32, 235);"
        "  border: 1px solid #4c453d;"
        "  border-radius: 2px;"
        "  padding: 6px 10px;"
        "}"
    );
    m_topPanel->setStyleSheet(overlayStyle);
    m_bottomPanel->setStyleSheet(overlayStyle);
    m_toast->setStyleSheet(overlayStyle);
    m_topPanel->raise();
    m_bottomPanel->raise();
    setCentralWidget(container);

    connect(loadButton, &QPushButton::clicked, this, &MainWindow::loadMagnet);
    connect(m_magnetInput, &QLineEdit::returnPressed, this, &MainWindow::loadMagnet);
    connect(m_streamButton, &QPushButton::clicked, this, &MainWindow::startTorrentPlayback);
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::togglePlayback);
    connect(m_volumeButton, &QPushButton::clicked, this, &MainWindow::toggleMute);
    connect(fullscreen, &QPushButton::clicked, this, &MainWindow::toggleFullscreen);
    connect(settingsButton, &QPushButton::clicked, this, [this, settingsButton] {
        // Anchored above the button and right-aligned with it; Qt would
        // otherwise drop the menu off the bottom of the screen in fullscreen.
        const QPoint corner = settingsButton->mapToGlobal(QPoint(0, 0));
        const QSize size = m_settingsMenu->sizeHint();
        m_settingsMenu->popup(QPoint(
            corner.x() + settingsButton->width() - size.width(),
            corner.y() - size.height() - 6
        ));
    });
    connect(m_volumeSlider, &QSlider::valueChanged, m_player, &MpvVideoWidget::setVolume);
    connect(m_seekSlider, &QSlider::sliderReleased, this, [this] {
        if (m_duration > 0.0) {
            m_player->seekTo(
                m_duration * static_cast<double>(m_seekSlider->value()) / 1000.0
            );
        }
    });
}

void MainWindow::buildSettingsMenu()
{
    m_settingsMenu = new QMenu(this);

    m_audioMenu = m_settingsMenu->addMenu(QStringLiteral("Audio track"));
    m_audioMenu->setEnabled(false);
    m_subtitleMenu = m_settingsMenu->addMenu(QStringLiteral("Subtitle track"));
    m_subtitleMenu->setEnabled(false);
    connect(
        m_settingsMenu->addAction(QStringLiteral("Add subtitle file…")),
        &QAction::triggered,
        this,
        &MainWindow::chooseSubtitleFile
    );

    m_settingsMenu->addSeparator();
    m_contentFitMenu = addChoiceMenu(QStringLiteral("Content fit"), {
        {QStringLiteral("Fit"), QStringLiteral("fit")},
        {QStringLiteral("Cover"), QStringLiteral("cover")},
        {QStringLiteral("Stretch"), QStringLiteral("stretch")},
        {QStringLiteral("Original"), QStringLiteral("original")},
    });
    connect(m_contentFitMenu, &QMenu::triggered, this, [this](QAction *action) {
        m_player->setContentFit(action->data().toString());
    });

    m_anime4kMenu = addChoiceMenu(QStringLiteral("Anime4K"), {
        {QStringLiteral("Off"), QStringLiteral("off")},
        {QStringLiteral("Fast"), QStringLiteral("fast")},
        {QStringLiteral("Quality"), QStringLiteral("quality")},
    });
    m_anime4kMenu->setEnabled(!anime4kDirectory().isEmpty());
    connect(m_anime4kMenu, &QMenu::triggered, this, [this](QAction *action) {
        applyAnime4kProfile(static_cast<int>(m_anime4kMenu->actions().indexOf(action)));
    });

    m_settingsMenu->addSeparator();
    // Tab-separated so the menu renders "D" in its shortcut column without
    // registering a real QShortcut, which would fire while typing a magnet.
    m_diagnosticsAction = m_settingsMenu->addAction(QStringLiteral("Diagnostics\tD"));
    m_diagnosticsAction->setCheckable(true);
    connect(
        m_diagnosticsAction,
        &QAction::toggled,
        this,
        &MainWindow::setDiagnosticsVisible
    );
}

QMenu *MainWindow::addChoiceMenu(
    const QString &title,
    const QList<QPair<QString, QString>> &entries
)
{
    QMenu *menu = m_settingsMenu->addMenu(title);
    auto *group = new QActionGroup(menu);
    group->setExclusive(true);
    for (const QPair<QString, QString> &entry : entries) {
        QAction *action = menu->addAction(entry.first);
        action->setCheckable(true);
        action->setData(entry.second);
        group->addAction(action);
    }
    menu->actions().constFirst()->setChecked(true);
    return menu;
}

void MainWindow::updateTimeLabel()
{
    const QString text =
        QStringLiteral("%1 / %2").arg(formatTime(m_position), formatTime(m_duration));
    m_timeLabel->setText(text);
    m_diagnostics->setPosition(text);
}

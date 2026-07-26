#pragma once

#include <QIcon>
#include <QMainWindow>

#include "player/MpvVideoWidget.hpp"
#include "torrent/TorrentSession.hpp"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QStatusBar;
class QTimer;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    MpvVideoWidget *player() const;
    void openLocalFile(const QString &path);
    void openMagnet(const QString &magnet);
    void setAutoStream(bool enabled);
    void setAnime4kProfile(const QString &profile);
    void setContentFit(const QString &mode);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void loadMagnet();
    void startTorrentPlayback();
    void updateTorrentFiles(const QList<TorrentFile> &files);
    void chooseSubtitleFile();
    void togglePlayback();
    void toggleFullscreen();
    void updatePosition(double seconds);
    void updateDuration(double seconds);
    void updatePaused(bool paused);
    void updateTracks(const QList<MpvTrack> &tracks);
    void applyAnime4kProfile(int index);

private:
    static QString formatTime(double seconds);
    static QString trackLabel(const MpvTrack &track);
    static QString anime4kDirectory();
    static QStringList anime4kShaderFiles(int profileIndex);
    void buildInterface();
    void updateTimeLabel();
    void showControls();
    void seekBy(double seconds);
    void adjustVolume(int delta);
    void toggleMute();
    void setControlsVisible(bool visible);
    bool shouldKeepControlsVisible() const;

    MpvVideoWidget *m_player = nullptr;
    TorrentSession *m_torrentSession = nullptr;
    QLineEdit *m_magnetInput = nullptr;
    QComboBox *m_videoFiles = nullptr;
    QPushButton *m_streamButton = nullptr;
    QPushButton *m_playButton = nullptr;
    QSlider *m_seekSlider = nullptr;
    QSlider *m_volumeSlider = nullptr;
    QLabel *m_timeLabel = nullptr;
    QComboBox *m_audioTracks = nullptr;
    QComboBox *m_subtitleTracks = nullptr;
    QComboBox *m_anime4kProfile = nullptr;
    QComboBox *m_contentFit = nullptr;
    QLabel *m_playbackStatus = nullptr;
    QStatusBar *m_statusBar = nullptr;
    QWidget *m_topPanel = nullptr;
    QWidget *m_bottomPanel = nullptr;
    QTimer *m_hideControlsTimer = nullptr;
    QTimer *m_clickTimer = nullptr;
    QIcon m_playIcon;
    QIcon m_pauseIcon;
    double m_position = 0.0;
    double m_duration = 0.0;
    bool m_paused = false;
    bool m_autoStream = false;
    bool m_torrentFileLoaded = false;
    bool m_muted = false;
};

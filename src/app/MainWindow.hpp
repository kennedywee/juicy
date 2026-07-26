#pragma once

#include <QIcon>
#include <QMainWindow>

#include "app/DiagnosticsPanel.hpp"
#include "app/SeekSlider.hpp"
#include "player/MpvVideoWidget.hpp"
#include "torrent/TorrentSession.hpp"

class QActionGroup;
class QComboBox;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QSlider;
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
    void autoLoadMagnet();
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
    void buildSettingsMenu();
    QMenu *addChoiceMenu(
        const QString &title,
        const QList<QPair<QString, QString>> &entries
    );
    void populateTrackMenu(
        QMenu *menu,
        QActionGroup *&group,
        const QList<MpvTrack> &tracks,
        const QString &type,
        bool includeOff,
        void (MpvVideoWidget::*select)(qint64)
    );
    void updateTimeLabel();
    void showControls();
    void showToast(const QString &message);
    void beginAutoPlay();
    void abandonAutoPlay();
    void startBlinking(QPushButton *button);
    void stopBlinking();
    void setDiagnosticsVisible(bool visible);
    void seekBy(double seconds);
    void adjustVolume(int delta);
    void toggleMute();
    void setControlsVisible(bool visible);
    bool shouldKeepControlsVisible() const;

    MpvVideoWidget *m_player = nullptr;
    TorrentSession *m_torrentSession = nullptr;
    DiagnosticsPanel *m_diagnostics = nullptr;
    QLineEdit *m_magnetInput = nullptr;
    QComboBox *m_videoFiles = nullptr;
    QPushButton *m_loadButton = nullptr;
    QPushButton *m_streamButton = nullptr;
    QPushButton *m_blinkTarget = nullptr;
    QPushButton *m_playButton = nullptr;
    QPushButton *m_volumeButton = nullptr;
    SeekSlider *m_seekSlider = nullptr;
    QSlider *m_volumeSlider = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_toast = nullptr;
    QMenu *m_settingsMenu = nullptr;
    QMenu *m_audioMenu = nullptr;
    QMenu *m_subtitleMenu = nullptr;
    QMenu *m_contentFitMenu = nullptr;
    QMenu *m_anime4kMenu = nullptr;
    QActionGroup *m_audioGroup = nullptr;
    QActionGroup *m_subtitleGroup = nullptr;
    QAction *m_diagnosticsAction = nullptr;
    QWidget *m_topPanel = nullptr;
    QWidget *m_bottomPanel = nullptr;
    QTimer *m_hideControlsTimer = nullptr;
    QTimer *m_toastTimer = nullptr;
    QTimer *m_pasteTimer = nullptr;
    QTimer *m_blinkTimer = nullptr;
    QTimer *m_clickTimer = nullptr;
    QIcon m_playIcon;
    QIcon m_pauseIcon;
    QIcon m_volumeIcon;
    QIcon m_mutedIcon;
    QString m_loadedMagnet;
    double m_position = 0.0;
    double m_duration = 0.0;
    bool m_paused = false;
    bool m_autoStream = false;
    bool m_torrentFileLoaded = false;
    bool m_muted = false;
    bool m_autoPlayPending = false;
    bool m_blinkOn = false;
};

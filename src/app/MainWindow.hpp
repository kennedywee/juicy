#pragma once

#include <QMainWindow>

#include "player/MpvVideoWidget.hpp"

class QComboBox;
class QLabel;
class QPushButton;
class QSlider;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    MpvVideoWidget *player() const;
    void openLocalFile(const QString &path);

private slots:
    void chooseLocalFile();
    void chooseSubtitleFile();
    void togglePlayback();
    void toggleFullscreen();
    void updatePosition(double seconds);
    void updateDuration(double seconds);
    void updatePaused(bool paused);
    void updateTracks(const QList<MpvTrack> &tracks);

private:
    static QString formatTime(double seconds);
    static QString trackLabel(const MpvTrack &track);
    void buildInterface();
    void updateTimeLabel();

    MpvVideoWidget *m_player = nullptr;
    QPushButton *m_playButton = nullptr;
    QSlider *m_seekSlider = nullptr;
    QLabel *m_timeLabel = nullptr;
    QComboBox *m_audioTracks = nullptr;
    QComboBox *m_subtitleTracks = nullptr;
    QComboBox *m_anime4kProfile = nullptr;
    double m_position = 0.0;
    double m_duration = 0.0;
    bool m_paused = false;
};


#pragma once

#include <QFrame>

#include "torrent/TorrentSession.hpp"

class QGridLayout;
class QLabel;

// Floating stats readout docked to the top-right of the video. Deliberately
// outside the auto-hiding control overlay: it stays put while the bar fades.
class DiagnosticsPanel final : public QFrame
{
    Q_OBJECT

public:
    explicit DiagnosticsPanel(QWidget *parent = nullptr);

    void setPlaybackState(const QString &state);
    void setPosition(const QString &text);
    void setTorrentStatus(const QString &status);
    void setTorrentStats(const TorrentStats &stats);
    void setError(const QString &message);

signals:
    void closeRequested();

private:
    QLabel *addRow(QGridLayout *grid, const QString &name);
    void addSection(QGridLayout *grid, const QString &title);

    QLabel *m_state = nullptr;
    QLabel *m_position = nullptr;
    QLabel *m_torrentStatus = nullptr;
    QLabel *m_rate = nullptr;
    QLabel *m_peers = nullptr;
    QLabel *m_downloaded = nullptr;
    QLabel *m_error = nullptr;
};

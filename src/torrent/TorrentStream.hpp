#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>

#include <QString>

#include <libtorrent/torrent_handle.hpp>

namespace libtorrent {
class torrent_info;
}

class TorrentFileStream;

class TorrentContent final : public std::enable_shared_from_this<TorrentContent>
{
public:
    TorrentContent(
        libtorrent::torrent_handle handle,
        const std::shared_ptr<const libtorrent::torrent_info> &information,
        int fileIndex,
        const QString &savePath
    );
    ~TorrentContent();

    TorrentContent(const TorrentContent &) = delete;
    TorrentContent &operator=(const TorrentContent &) = delete;

    std::unique_ptr<TorrentFileStream> openStream();
    void notifyPieceAvailable();
    void prepareForPlayback();
    bool playbackReady() const;
    void stop();

    qint64 size() const;
    QString filePath() const;

private:
    friend class TorrentFileStream;

    bool waitForByte(qint64 fileOffset, const std::atomic_bool &cancelled);
    void prioritizeFrom(qint64 fileOffset);
    qint64 bytesUntilPieceEnd(qint64 fileOffset) const;

    libtorrent::torrent_handle m_handle;
    QString m_filePath;
    qint64 m_fileStart = 0;
    qint64 m_fileSize = 0;
    int m_pieceLength = 0;
    int m_numPieces = 0;
    std::mutex m_waitMutex;
    std::condition_variable m_pieceChanged;
    std::atomic_bool m_stopping = false;
    std::atomic_int m_priorityAnchor = -1;
};

class TorrentFileStream final
{
public:
    explicit TorrentFileStream(std::shared_ptr<TorrentContent> content);
    ~TorrentFileStream();

    TorrentFileStream(const TorrentFileStream &) = delete;
    TorrentFileStream &operator=(const TorrentFileStream &) = delete;

    std::int64_t read(char *buffer, std::uint64_t byteCount);
    std::int64_t seek(std::int64_t offset);
    std::int64_t size() const;
    void cancel();

private:
    bool ensureOpen();

    std::shared_ptr<TorrentContent> m_content;
    std::mutex m_positionMutex;
    qint64 m_position = 0;
    int m_fileDescriptor = -1;
    std::atomic_bool m_cancelled = false;
    int m_traceEvents = 0;
};

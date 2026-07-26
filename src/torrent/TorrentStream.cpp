#include "torrent/TorrentStream.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstddef>
#include <fcntl.h>
#include <utility>
#include <unistd.h>

#include <QDir>

#include <libtorrent/torrent_info.hpp>

namespace lt = libtorrent;

namespace {

constexpr qint64 kReadAheadBytes = 32LL * 1024LL * 1024LL;
constexpr int kMaximumDeadlineMs = 10'000;

} // namespace

TorrentContent::TorrentContent(
    lt::torrent_handle handle,
    const std::shared_ptr<const lt::torrent_info> &information,
    int fileIndex,
    const QString &savePath
)
    : m_handle(std::move(handle))
{
    const lt::file_index_t index(fileIndex);
    const lt::file_storage &storage = information->layout();
    m_filePath = QDir(savePath).filePath(QString::fromStdString(storage.file_path(index)));
    m_fileStart = storage.file_offset(index);
    m_fileSize = storage.file_size(index);
    m_pieceLength = information->piece_length();
    m_numPieces = information->num_pieces();
}

TorrentContent::~TorrentContent()
{
    stop();
}

std::unique_ptr<TorrentFileStream> TorrentContent::openStream()
{
    return std::make_unique<TorrentFileStream>(shared_from_this());
}

void TorrentContent::notifyPieceAvailable()
{
    m_pieceChanged.notify_all();
}

void TorrentContent::stop()
{
    m_stopping.store(true);
    m_pieceChanged.notify_all();
}

qint64 TorrentContent::size() const
{
    return m_fileSize;
}

QString TorrentContent::filePath() const
{
    return m_filePath;
}

bool TorrentContent::waitForByte(qint64 fileOffset, const std::atomic_bool &cancelled)
{
    if (fileOffset < 0 || fileOffset >= m_fileSize || m_pieceLength <= 0) {
        return false;
    }

    const qint64 torrentOffset = m_fileStart + fileOffset;
    const int piece = static_cast<int>(torrentOffset / m_pieceLength);
    if (piece < 0 || piece >= m_numPieces) {
        return false;
    }

    prioritizeFrom(fileOffset);
    const lt::piece_index_t pieceIndex(piece);
    std::unique_lock lock(m_waitMutex);
    while (!m_stopping.load() && !cancelled.load()) {
        if (m_handle.is_valid() && m_handle.have_piece(pieceIndex)) {
            return true;
        }
        m_pieceChanged.wait_for(lock, std::chrono::milliseconds(200));
    }
    return false;
}

void TorrentContent::prioritizeFrom(qint64 fileOffset)
{
    if (!m_handle.is_valid() || m_pieceLength <= 0) {
        return;
    }

    const qint64 torrentOffset = m_fileStart + qBound<qint64>(0, fileOffset, m_fileSize);
    const int firstPiece = static_cast<int>(torrentOffset / m_pieceLength);
    const qint64 lastByte = std::min(
        m_fileStart + m_fileSize - 1,
        torrentOffset + kReadAheadBytes
    );
    const int lastPiece = std::min(
        m_numPieces - 1,
        static_cast<int>(lastByte / m_pieceLength)
    );

    m_handle.clear_piece_deadlines();
    for (int piece = firstPiece; piece <= lastPiece; ++piece) {
        const int distance = piece - firstPiece;
        const int deadline = std::min(kMaximumDeadlineMs, distance * 250);
        m_handle.set_piece_deadline(lt::piece_index_t(piece), deadline);
    }
}

qint64 TorrentContent::bytesUntilPieceEnd(qint64 fileOffset) const
{
    const qint64 torrentOffset = m_fileStart + fileOffset;
    const qint64 nextPieceOffset = ((torrentOffset / m_pieceLength) + 1) * m_pieceLength;
    return nextPieceOffset - torrentOffset;
}

TorrentFileStream::TorrentFileStream(std::shared_ptr<TorrentContent> content)
    : m_content(std::move(content))
{
}

TorrentFileStream::~TorrentFileStream()
{
    cancel();
    if (m_fileDescriptor >= 0) {
        ::close(m_fileDescriptor);
        m_fileDescriptor = -1;
    }
}

std::int64_t TorrentFileStream::read(char *buffer, std::uint64_t byteCount)
{
    if (buffer == nullptr || byteCount == 0 || m_cancelled.load()) {
        return byteCount == 0 ? 0 : -1;
    }

    std::scoped_lock positionLock(m_positionMutex);
    if (m_position >= m_content->size()) {
        return 0;
    }
    if (!m_content->waitForByte(m_position, m_cancelled) || !ensureOpen()) {
        return -1;
    }

    const qint64 remaining = m_content->size() - m_position;
    const qint64 pieceRemaining = m_content->bytesUntilPieceEnd(m_position);
    const qint64 requested = std::min<qint64>(
        {
            remaining,
            pieceRemaining,
            static_cast<qint64>(std::min<std::uint64_t>(byteCount, INT_MAX)),
        }
    );
    ssize_t bytesRead = -1;
    do {
        bytesRead = ::pread(
            m_fileDescriptor,
            buffer,
            static_cast<std::size_t>(requested),
            static_cast<off_t>(m_position)
        );
    } while (bytesRead < 0 && errno == EINTR);
    if (bytesRead < 0) {
        return -1;
    }
    m_position += bytesRead;
    return bytesRead;
}

std::int64_t TorrentFileStream::seek(std::int64_t offset)
{
    if (offset < 0 || offset > m_content->size() || m_cancelled.load()) {
        return -1;
    }

    std::scoped_lock lock(m_positionMutex);
    m_position = offset;
    if (offset < m_content->size()) {
        m_content->prioritizeFrom(offset);
    }
    return m_position;
}

std::int64_t TorrentFileStream::size() const
{
    return m_content->size();
}

void TorrentFileStream::cancel()
{
    m_cancelled.store(true);
    m_content->notifyPieceAvailable();
}

bool TorrentFileStream::ensureOpen()
{
    if (m_fileDescriptor >= 0) {
        return true;
    }
    const QByteArray encodedPath = m_content->filePath().toLocal8Bit();
    m_fileDescriptor = ::open(encodedPath.constData(), O_RDONLY | O_CLOEXEC);
    return m_fileDescriptor >= 0;
}

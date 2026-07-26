#include "torrent/TorrentSession.hpp"
#include "torrent/TorrentStream.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QTimer>

#include <libtorrent/alert_types.hpp>
#include <libtorrent/download_priority.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/session.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/torrent_flags.hpp>
#include <libtorrent/torrent_info.hpp>

namespace lt = libtorrent;

namespace {

const QSet<QString> kVideoExtensions {
    QStringLiteral("mkv"),
    QStringLiteral("mp4"),
    QStringLiteral("webm"),
    QStringLiteral("avi"),
    QStringLiteral("mov"),
    QStringLiteral("m4v"),
    QStringLiteral("ts"),
    QStringLiteral("m2ts"),
    QStringLiteral("ogm"),
};

QString humanRate(int bytesPerSecond)
{
    constexpr double bytesPerMebibyte = 1024.0 * 1024.0;
    const double mebibytes = static_cast<double>(bytesPerSecond) / bytesPerMebibyte;
    return QStringLiteral("%1 MiB/s").arg(mebibytes, 0, 'f', 1);
}

bool isVideoFile(const QString &path)
{
    return kVideoExtensions.contains(QFileInfo(path).suffix().toLower());
}

QString temporaryDirectoryTemplate()
{
    QDir temporaryRoot(QDir::tempPath());
    const QRegularExpression sessionPattern(
        QStringLiteral("^juicy-player-(\\d+)-[A-Za-z0-9]{6}$")
    );
    const QStringList candidates = temporaryRoot.entryList(
        {QStringLiteral("juicy-player-*-*")},
        QDir::Dirs | QDir::NoDotAndDotDot
    );
    for (const QString &candidate : candidates) {
        const QRegularExpressionMatch match = sessionPattern.match(candidate);
        if (!match.hasMatch()) {
            continue;
        }

        const QString processPath = QStringLiteral("/proc/%1").arg(match.captured(1));
        if (!QFileInfo::exists(processPath)) {
            QDir(temporaryRoot.filePath(candidate)).removeRecursively();
        }
    }

    return temporaryRoot.filePath(
        QStringLiteral("juicy-player-%1-XXXXXX")
            .arg(QCoreApplication::applicationPid())
    );
}

} // namespace

struct TorrentSession::Impl
{
    explicit Impl(TorrentSession *owner)
        : temporaryDirectory(temporaryDirectoryTemplate())
        , q(owner)
    {
        if (!temporaryDirectory.isValid()) {
            initializationError = QStringLiteral("Unable to create temporary torrent storage.");
            return;
        }

        try {
            lt::settings_pack settings;
            const auto categories = lt::alert_category::error
                | lt::alert_category::status
                | lt::alert_category::storage
                | lt::alert_category::piece_progress;
            using AlertBits = lt::alert_category_t::underlying_type;
            settings.set_int(
                lt::settings_pack::alert_mask,
                static_cast<int>(static_cast<AlertBits>(categories))
            );
            settings.set_bool(lt::settings_pack::enable_dht, true);
            settings.set_bool(lt::settings_pack::enable_lsd, true);
            settings.set_bool(lt::settings_pack::enable_upnp, true);
            settings.set_bool(lt::settings_pack::enable_natpmp, true);
            session = std::make_unique<lt::session>(settings);
        } catch (const std::exception &exception) {
            initializationError = QStringLiteral("Unable to initialize libtorrent: %1")
                .arg(QString::fromUtf8(exception.what()));
        }
    }

    void publishFiles()
    {
        files.clear();
        if (!handle.is_valid()) {
            return;
        }

        const std::shared_ptr<const lt::torrent_info> information = handle.torrent_file();
        if (!information) {
            return;
        }

        const lt::file_storage &storage = information->layout();
        for (lt::file_index_t index{0}; index < storage.end_file(); ++index) {
            const QString path = QString::fromStdString(storage.file_path(index));
            if (!storage.pad_file_at(index) && storage.file_size(index) > 0
                && isVideoFile(path)) {
                files.push_back(TorrentFile {
                    .index = static_cast<int>(index),
                    .name = QFileInfo(path).fileName(),
                    .path = path,
                    .size = storage.file_size(index),
                });
            }
        }

        std::sort(files.begin(), files.end(), [](const TorrentFile &left, const TorrentFile &right) {
            return left.path.localeAwareCompare(right.path) < 0;
        });
        emit q->filesReady(files);
        if (files.isEmpty()) {
            emit q->errorOccurred(QStringLiteral("This torrent does not contain a supported video file."));
        } else {
            emit q->statusChanged(
                QStringLiteral("Metadata loaded · %1 video file(s)").arg(files.size())
            );
        }
    }

    QTemporaryDir temporaryDirectory;
    TorrentSession *q = nullptr;
    std::unique_ptr<lt::session> session;
    lt::torrent_handle handle;
    std::shared_ptr<TorrentContent> content;
    QList<TorrentFile> files;
    QString initializationError;
    QString currentSavePath;
    int torrentSequence = 0;
};

TorrentSession::TorrentSession(QObject *parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>(this))
    , m_alertTimer(new QTimer(this))
    , m_statusTimer(new QTimer(this))
{
    m_alertTimer->setInterval(100);
    m_statusTimer->setInterval(500);
    connect(m_alertTimer, &QTimer::timeout, this, &TorrentSession::processAlerts);
    connect(m_statusTimer, &QTimer::timeout, this, &TorrentSession::updateStatus);
    if (m_impl->session) {
        m_alertTimer->start();
        m_statusTimer->start();
    }
}

TorrentSession::~TorrentSession()
{
    if (m_impl->content) {
        m_impl->content->stop();
        m_impl->content.reset();
    }
    m_impl->session.reset();
}

bool TorrentSession::addMagnet(const QString &magnet)
{
    if (!m_impl->session) {
        emit errorOccurred(m_impl->initializationError);
        return false;
    }

    lt::error_code error;
    lt::add_torrent_params parameters = lt::parse_magnet_uri(magnet.toStdString(), error);
    if (error) {
        emit errorOccurred(
            QStringLiteral("Invalid magnet link: %1").arg(QString::fromStdString(error.message()))
        );
        return false;
    }

    if (m_impl->handle.is_valid()) {
        if (m_impl->content) {
            m_impl->content->stop();
            m_impl->content.reset();
        }
        m_impl->session->remove_torrent(m_impl->handle);
        m_impl->handle = {};
        m_impl->files.clear();
    }

    ++m_impl->torrentSequence;
    m_impl->currentSavePath = QDir(m_impl->temporaryDirectory.path()).filePath(
        QStringLiteral("torrent-%1").arg(m_impl->torrentSequence)
    );
    if (!QDir().mkpath(m_impl->currentSavePath)) {
        emit errorOccurred(QStringLiteral("Unable to create temporary torrent storage."));
        return false;
    }
    parameters.save_path = m_impl->currentSavePath.toStdString();
    parameters.flags &= ~lt::torrent_flags::paused;
    parameters.flags |= lt::torrent_flags::auto_managed;
    m_impl->session->async_add_torrent(std::move(parameters));
    emit statusChanged(QStringLiteral("Loading torrent metadata…"));
    return true;
}

bool TorrentSession::selectFile(int fileIndex)
{
    if (!m_impl->handle.is_valid()) {
        emit errorOccurred(QStringLiteral("Torrent metadata is not ready."));
        return false;
    }

    const std::shared_ptr<const lt::torrent_info> information = m_impl->handle.torrent_file();
    if (!information || fileIndex < 0 || fileIndex >= static_cast<int>(information->num_files())) {
        emit errorOccurred(QStringLiteral("The selected torrent file is invalid."));
        return false;
    }

    std::vector<lt::download_priority_t> priorities(
        static_cast<std::size_t>(information->num_files()),
        lt::dont_download
    );
    priorities[static_cast<std::size_t>(fileIndex)] = lt::default_priority;
    m_impl->handle.prioritize_files(priorities);

    const auto match = std::find_if(
        m_impl->files.cbegin(),
        m_impl->files.cend(),
        [fileIndex](const TorrentFile &file) {
            return file.index == fileIndex;
        }
    );
    if (match == m_impl->files.cend()) {
        emit errorOccurred(QStringLiteral("The selected file is not a supported video."));
        return false;
    }

    m_impl->content = std::make_shared<TorrentContent>(
        m_impl->handle,
        information,
        fileIndex,
        m_impl->currentSavePath
    );
    emit fileSelected(*match);
    emit streamReady(m_impl->content, *match);
    emit statusChanged(QStringLiteral("Selected %1").arg(match->name));
    return true;
}

QString TorrentSession::temporaryPath() const
{
    return m_impl->temporaryDirectory.path();
}

void TorrentSession::processAlerts()
{
    if (!m_impl->session) {
        return;
    }

    std::vector<lt::alert *> alerts;
    m_impl->session->pop_alerts(&alerts);
    for (lt::alert *alert : alerts) {
        if (const auto *added = lt::alert_cast<lt::add_torrent_alert>(alert)) {
            if (added->error) {
                emit errorOccurred(
                    QStringLiteral("Unable to add torrent: %1")
                        .arg(QString::fromStdString(added->error.message()))
                );
                continue;
            }
            m_impl->handle = added->handle;
            if (m_impl->handle.status().has_metadata) {
                m_impl->publishFiles();
            }
        } else if (const auto *metadata = lt::alert_cast<lt::metadata_received_alert>(alert)) {
            if (metadata->handle == m_impl->handle) {
                m_impl->publishFiles();
            }
        } else if (const auto *torrentError = lt::alert_cast<lt::torrent_error_alert>(alert)) {
            if (torrentError->handle == m_impl->handle) {
                emit errorOccurred(
                    QStringLiteral("Torrent error: %1")
                        .arg(QString::fromStdString(torrentError->error.message()))
                );
            }
        } else if (const auto *piece = lt::alert_cast<lt::piece_finished_alert>(alert)) {
            if (piece->handle == m_impl->handle && m_impl->content) {
                m_impl->content->notifyPieceAvailable();
            }
        }
    }
}

void TorrentSession::updateStatus()
{
    if (!m_impl->handle.is_valid()) {
        return;
    }

    const lt::torrent_status status = m_impl->handle.status(
        lt::torrent_handle::query_accurate_download_counters
    );
    if (!status.has_metadata) {
        emit statusChanged(
            QStringLiteral("Loading metadata · %1 peer(s)").arg(status.num_peers)
        );
        return;
    }

    emit statusChanged(
        QStringLiteral("%1 · %2 peer(s) · %3 downloaded")
            .arg(humanRate(status.download_rate))
            .arg(status.num_peers)
            .arg(QString::number(
                static_cast<double>(status.total_done) / (1024.0 * 1024.0),
                'f',
                1
            ) + QStringLiteral(" MiB"))
    );
}

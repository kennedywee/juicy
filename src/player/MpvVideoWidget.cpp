#include "player/MpvVideoWidget.hpp"
#include "torrent/TorrentStream.hpp"

#include <array>
#include <vector>

#include <QByteArray>
#include <QDebug>
#include <QMetaObject>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QTimer>

extern "C" {
#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>
#include <mpv/stream_cb.h>
}

namespace {

constexpr std::array<const char *, 8> kMpvOptions {
    "terminal=no",
    "msg-level=all=warn",
    "vo=libmpv",
    "hwdec=auto-safe",
    "keep-open=yes",
    "osc=no",
    "background=color",
    "background-color=#FF000000",
};

const mpv_node *mapValue(const mpv_node &map, const char *key)
{
    if (map.format != MPV_FORMAT_NODE_MAP || map.u.list == nullptr) {
        return nullptr;
    }

    for (int index = 0; index < map.u.list->num; ++index) {
        if (qstrcmp(map.u.list->keys[index], key) == 0) {
            return &map.u.list->values[index];
        }
    }
    return nullptr;
}

QString nodeString(const mpv_node *node)
{
    if (node == nullptr || node->format != MPV_FORMAT_STRING || node->u.string == nullptr) {
        return {};
    }
    return QString::fromUtf8(node->u.string);
}

qint64 nodeInteger(const mpv_node *node)
{
    if (node == nullptr || node->format != MPV_FORMAT_INT64) {
        return 0;
    }
    return node->u.int64;
}

bool nodeFlag(const mpv_node *node)
{
    return node != nullptr && node->format == MPV_FORMAT_FLAG && node->u.flag != 0;
}

} // namespace

MpvVideoWidget::MpvVideoWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
    initializeMpv();
}

MpvVideoWidget::~MpvVideoWidget()
{
    if (m_renderContext != nullptr) {
        makeCurrent();
        mpv_render_context_set_update_callback(m_renderContext, nullptr, nullptr);
        mpv_render_context_free(m_renderContext);
        m_renderContext = nullptr;
        doneCurrent();
    }

    if (m_mpv != nullptr) {
        mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
    std::scoped_lock lock(m_torrentContentMutex);
    m_torrentContent.reset();
}

void MpvVideoWidget::loadFile(const QString &path)
{
    m_currentFile = path;
    ++m_loadGeneration;
    m_torrentRetryCount = 0;
    m_currentFileLoaded = false;
    if (m_renderContext == nullptr) {
        m_pendingFile = path;
        return;
    }

    loadCurrentFile();
}

void MpvVideoWidget::setPaused(bool paused)
{
    issueCommand({
        QStringLiteral("set"),
        QStringLiteral("pause"),
        paused ? QStringLiteral("yes") : QStringLiteral("no"),
    });
}

void MpvVideoWidget::seekTo(double seconds)
{
    issueCommand({
        QStringLiteral("seek"),
        QString::number(seconds, 'f', 3),
        QStringLiteral("absolute+exact"),
    });
}

void MpvVideoWidget::setVolume(int percent)
{
    issueCommand({
        QStringLiteral("set"),
        QStringLiteral("volume"),
        QString::number(qBound(0, percent, 100)),
    });
}

void MpvVideoWidget::selectAudioTrack(qint64 id)
{
    issueCommand({
        QStringLiteral("set"),
        QStringLiteral("aid"),
        id > 0 ? QString::number(id) : QStringLiteral("auto"),
    });
}

void MpvVideoWidget::selectSubtitleTrack(qint64 id)
{
    issueCommand({
        QStringLiteral("set"),
        QStringLiteral("sid"),
        id > 0 ? QString::number(id) : QStringLiteral("no"),
    });
}

void MpvVideoWidget::addSubtitleFile(const QString &path)
{
    issueCommand({
        QStringLiteral("sub-add"),
        path,
        QStringLiteral("select"),
    });
}

void MpvVideoWidget::setShaderFiles(const QStringList &paths)
{
    issueCommand({
        QStringLiteral("change-list"),
        QStringLiteral("glsl-shaders"),
        QStringLiteral("clr"),
        QString(),
    });
    for (const QString &path : paths) {
        issueCommand({
            QStringLiteral("change-list"),
            QStringLiteral("glsl-shaders"),
            QStringLiteral("append"),
            path,
        });
    }
}

void MpvVideoWidget::setTorrentContent(const std::shared_ptr<TorrentContent> &content)
{
    std::scoped_lock lock(m_torrentContentMutex);
    m_torrentContent = content;
}

void MpvVideoWidget::initializeGL()
{
    if (m_mpv == nullptr) {
        return;
    }

    mpv_opengl_init_params openGlParameters {
        .get_proc_address = &MpvVideoWidget::resolveOpenGlSymbol,
        .get_proc_address_ctx = QOpenGLContext::currentContext(),
    };
    const char *apiType = MPV_RENDER_API_TYPE_OPENGL;
    mpv_render_param parameters[] {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(apiType)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &openGlParameters},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    const int result = mpv_render_context_create(&m_renderContext, m_mpv, parameters);
    if (result < 0) {
        reportMpvError(QStringLiteral("Creating the libmpv render context"), result);
        return;
    }

    mpv_render_context_set_update_callback(
        m_renderContext,
        &MpvVideoWidget::handleRenderUpdate,
        this
    );

    if (!m_pendingFile.isEmpty()) {
        const QString file = m_pendingFile;
        m_pendingFile.clear();
        loadFile(file);
    }
}

void MpvVideoWidget::paintGL()
{
    if (m_renderContext == nullptr) {
        return;
    }

    QOpenGLFunctions *functions = QOpenGLContext::currentContext()->functions();
    while (functions->glGetError() != GL_NO_ERROR) {
    }

    const qreal scale = devicePixelRatioF();
    mpv_opengl_fbo framebuffer {
        .fbo = static_cast<int>(defaultFramebufferObject()),
        .w = qRound(static_cast<qreal>(width()) * scale),
        .h = qRound(static_cast<qreal>(height()) * scale),
        .internal_format = 0,
    };
    int flipVertically = 1;
    mpv_render_param parameters[] {
        {MPV_RENDER_PARAM_OPENGL_FBO, &framebuffer},
        {MPV_RENDER_PARAM_FLIP_Y, &flipVertically},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    mpv_render_context_render(m_renderContext, parameters);
}

void MpvVideoWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit toggleFullscreenRequested();
        event->accept();
        return;
    }
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void MpvVideoWidget::drainEvents()
{
    if (m_mpv == nullptr) {
        return;
    }

    while (true) {
        const mpv_event *event = mpv_wait_event(m_mpv, 0.0);
        if (event == nullptr || event->event_id == MPV_EVENT_NONE) {
            break;
        }
        processEvent(*event);
    }
}

void *MpvVideoWidget::resolveOpenGlSymbol(void *context, const char *name)
{
    auto *openGlContext = static_cast<QOpenGLContext *>(context);
    if (openGlContext == nullptr) {
        return nullptr;
    }

    const QFunctionPointer address = openGlContext->getProcAddress(QByteArray(name));
    return reinterpret_cast<void *>(address);
}

void MpvVideoWidget::handleMpvWakeup(void *context)
{
    auto *widget = static_cast<MpvVideoWidget *>(context);
    QMetaObject::invokeMethod(widget, &MpvVideoWidget::drainEvents, Qt::QueuedConnection);
}

void MpvVideoWidget::handleRenderUpdate(void *context)
{
    auto *widget = static_cast<MpvVideoWidget *>(context);
    QMetaObject::invokeMethod(widget, [widget] {
        widget->update();
    }, Qt::QueuedConnection);
}

int MpvVideoWidget::openTorrentStream(
    void *context,
    char *uri,
    mpv_stream_cb_info *information
)
{
    auto *widget = static_cast<MpvVideoWidget *>(context);
    if (widget == nullptr || uri == nullptr || information == nullptr
        || !QByteArray(uri).startsWith("juicy://")) {
        return MPV_ERROR_LOADING_FAILED;
    }
    std::shared_ptr<TorrentContent> content;
    {
        std::scoped_lock lock(widget->m_torrentContentMutex);
        content = widget->m_torrentContent;
    }
    if (!content) {
        return MPV_ERROR_LOADING_FAILED;
    }

    std::unique_ptr<TorrentFileStream> stream = content->openStream();
    information->cookie = stream.release();
    information->read_fn = &MpvVideoWidget::readTorrentStream;
    information->seek_fn = &MpvVideoWidget::seekTorrentStream;
    information->size_fn = &MpvVideoWidget::sizeTorrentStream;
    information->cancel_fn = &MpvVideoWidget::cancelTorrentStream;
    information->close_fn = &MpvVideoWidget::closeTorrentStream;
    return 0;
}

std::int64_t MpvVideoWidget::readTorrentStream(
    void *stream,
    char *buffer,
    std::uint64_t byteCount
)
{
    return static_cast<TorrentFileStream *>(stream)->read(buffer, byteCount);
}

std::int64_t MpvVideoWidget::seekTorrentStream(void *stream, std::int64_t offset)
{
    return static_cast<TorrentFileStream *>(stream)->seek(offset);
}

std::int64_t MpvVideoWidget::sizeTorrentStream(void *stream)
{
    return static_cast<TorrentFileStream *>(stream)->size();
}

void MpvVideoWidget::cancelTorrentStream(void *stream)
{
    static_cast<TorrentFileStream *>(stream)->cancel();
}

void MpvVideoWidget::closeTorrentStream(void *stream)
{
    delete static_cast<TorrentFileStream *>(stream);
}

bool MpvVideoWidget::initializeMpv()
{
    m_mpv = mpv_create();
    if (m_mpv == nullptr) {
        reportMpvError(QStringLiteral("Creating a libmpv instance"), MPV_ERROR_GENERIC);
        return false;
    }

    for (const char *option : kMpvOptions) {
        const QByteArray value(option);
        const qsizetype separator = value.indexOf('=');
        const QByteArray name = value.first(separator);
        const QByteArray setting = value.sliced(separator + 1);
        const int result = mpv_set_option_string(m_mpv, name.constData(), setting.constData());
        if (result < 0) {
            reportMpvError(QStringLiteral("Setting mpv option %1").arg(QString::fromUtf8(name)), result);
            return false;
        }
    }

    const int streamResult = mpv_stream_cb_add_ro(
        m_mpv,
        "juicy",
        this,
        &MpvVideoWidget::openTorrentStream
    );
    if (streamResult < 0) {
        reportMpvError(QStringLiteral("Registering the torrent stream"), streamResult);
        return false;
    }

    const int result = mpv_initialize(m_mpv);
    if (result < 0) {
        reportMpvError(QStringLiteral("Initializing libmpv"), result);
        return false;
    }

    mpv_request_log_messages(m_mpv, "warn");
    mpv_observe_property(m_mpv, 1, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 2, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 3, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 4, "track-list", MPV_FORMAT_NODE);
    mpv_set_wakeup_callback(m_mpv, &MpvVideoWidget::handleMpvWakeup, this);
    return true;
}

bool MpvVideoWidget::issueCommand(const QStringList &arguments)
{
    if (m_mpv == nullptr || arguments.isEmpty()) {
        return false;
    }

    std::vector<QByteArray> encodedArguments;
    encodedArguments.reserve(static_cast<std::size_t>(arguments.size()));
    for (const QString &argument : arguments) {
        encodedArguments.push_back(argument.toUtf8());
    }

    std::vector<const char *> command;
    command.reserve(encodedArguments.size() + 1);
    for (const QByteArray &argument : encodedArguments) {
        command.push_back(argument.constData());
    }
    command.push_back(nullptr);

    const int result = mpv_command_async(m_mpv, 0, command.data());
    if (result < 0) {
        reportMpvError(QStringLiteral("Sending a command to libmpv"), result);
        return false;
    }
    return true;
}

void MpvVideoWidget::loadCurrentFile()
{
    m_currentFileLoaded = false;
    QString path = m_currentFile;
    if (path.startsWith(QStringLiteral("juicy://"))) {
        path += QStringLiteral("?attempt=%1").arg(m_torrentRetryCount);
    }
    issueCommand({QStringLiteral("loadfile"), path, QStringLiteral("replace")});
}

void MpvVideoWidget::processEvent(const mpv_event &event)
{
    switch (event.event_id) {
    case MPV_EVENT_FILE_LOADED:
        m_currentFileLoaded = true;
        refreshTracks();
        emit fileLoaded();
        break;
    case MPV_EVENT_END_FILE: {
        const auto *end = static_cast<const mpv_event_end_file *>(event.data);
        if (end != nullptr) {
            qInfo() << "libmpv end-file reason" << end->reason
                    << "error" << end->error
                    << "loaded" << m_currentFileLoaded;
            const bool torrentProbeFailed =
                m_currentFile.startsWith(QStringLiteral("juicy://"))
                && !m_currentFileLoaded
                && (end->reason == MPV_END_FILE_REASON_ERROR
                    || end->reason == MPV_END_FILE_REASON_EOF);
            if (torrentProbeFailed && m_torrentRetryCount < 2) {
                ++m_torrentRetryCount;
                const int generation = m_loadGeneration;
                emit playbackRetrying(m_torrentRetryCount);
                QTimer::singleShot(1000, this, [this, generation] {
                    if (generation == m_loadGeneration) {
                        loadCurrentFile();
                    }
                });
                break;
            }
            if (end->reason == MPV_END_FILE_REASON_ERROR) {
                emit playbackError(
                    QStringLiteral("Playback failed: %1")
                        .arg(QString::fromUtf8(mpv_error_string(end->error)))
                );
            } else if (torrentProbeFailed) {
                emit playbackError(
                    QStringLiteral("Playback ended before the video could be opened.")
                );
            }
        }
        break;
    }
    case MPV_EVENT_PROPERTY_CHANGE: {
        const auto *property = static_cast<const mpv_event_property *>(event.data);
        if (property == nullptr || property->name == nullptr) {
            break;
        }

        const QByteArray name(property->name);
        if (name == "time-pos" && property->format == MPV_FORMAT_DOUBLE
            && property->data != nullptr) {
            emit positionChanged(*static_cast<const double *>(property->data));
        } else if (name == "duration" && property->format == MPV_FORMAT_DOUBLE
                   && property->data != nullptr) {
            emit durationChanged(*static_cast<const double *>(property->data));
        } else if (name == "pause" && property->format == MPV_FORMAT_FLAG
                   && property->data != nullptr) {
            emit pauseChanged(*static_cast<const int *>(property->data) != 0);
        } else if (name == "track-list") {
            refreshTracks();
        }
        break;
    }
    case MPV_EVENT_LOG_MESSAGE: {
        const auto *message = static_cast<const mpv_event_log_message *>(event.data);
        if (message != nullptr) {
            qWarning().noquote() << QString::fromUtf8(message->prefix)
                                << QString::fromUtf8(message->text).trimmed();
        }
        break;
    }
    default:
        break;
    }
}

void MpvVideoWidget::refreshTracks()
{
    if (m_mpv == nullptr) {
        return;
    }

    mpv_node trackList {};
    const int result = mpv_get_property(m_mpv, "track-list", MPV_FORMAT_NODE, &trackList);
    if (result < 0) {
        return;
    }

    QList<MpvTrack> tracks;
    if (trackList.format == MPV_FORMAT_NODE_ARRAY && trackList.u.list != nullptr) {
        tracks.reserve(trackList.u.list->num);
        for (int index = 0; index < trackList.u.list->num; ++index) {
            const mpv_node &entry = trackList.u.list->values[index];
            MpvTrack track {
                .id = nodeInteger(mapValue(entry, "id")),
                .type = nodeString(mapValue(entry, "type")),
                .title = nodeString(mapValue(entry, "title")),
                .language = nodeString(mapValue(entry, "lang")),
                .selected = nodeFlag(mapValue(entry, "selected")),
                .external = nodeFlag(mapValue(entry, "external")),
            };
            if (track.id > 0 && (track.type == QStringLiteral("audio")
                                  || track.type == QStringLiteral("sub"))) {
                tracks.push_back(std::move(track));
            }
        }
    }
    mpv_free_node_contents(&trackList);
    emit tracksChanged(tracks);
}

void MpvVideoWidget::reportMpvError(const QString &operation, int errorCode)
{
    const QString message = QStringLiteral("%1 failed: %2")
        .arg(operation, QString::fromUtf8(mpv_error_string(errorCode)));
    qCritical().noquote() << message;
    // Queued so errors raised inside the constructor still reach listeners.
    QMetaObject::invokeMethod(this, [this, message] {
        emit fatalError(message);
    }, Qt::QueuedConnection);
}

#include "player/MpvVideoWidget.hpp"

#include <array>
#include <vector>

#include <QByteArray>
#include <QDebug>
#include <QMetaObject>
#include <QOpenGLContext>
#include <QThread>

extern "C" {
#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>
}

namespace {

constexpr std::array<const char *, 6> kMpvOptions {
    "terminal=no",
    "msg-level=all=warn",
    "vo=libmpv",
    "hwdec=auto-safe",
    "keep-open=yes",
    "osc=no",
};

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
}

void MpvVideoWidget::loadFile(const QString &path)
{
    if (m_renderContext == nullptr) {
        m_pendingFile = path;
        return;
    }

    issueCommand({QStringLiteral("loadfile"), path, QStringLiteral("replace")});
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

bool MpvVideoWidget::initializeMpv()
{
    m_mpv = mpv_create();
    if (m_mpv == nullptr) {
        emit fatalError(QStringLiteral("Unable to create a libmpv instance."));
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

    const int result = mpv_initialize(m_mpv);
    if (result < 0) {
        reportMpvError(QStringLiteral("Initializing libmpv"), result);
        return false;
    }

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

void MpvVideoWidget::processEvent(const mpv_event &event)
{
    switch (event.event_id) {
    case MPV_EVENT_FILE_LOADED:
        emit fileLoaded();
        break;
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

void MpvVideoWidget::reportMpvError(const QString &operation, int errorCode)
{
    emit fatalError(
        QStringLiteral("%1 failed: %2")
            .arg(operation, QString::fromUtf8(mpv_error_string(errorCode)))
    );
}


#pragma once

#include <cstdint>
#include <memory>
#include <mutex>

#include <QList>
#include <QOpenGLWidget>
#include <QString>

struct mpv_event;
struct mpv_handle;
struct mpv_render_context;
struct mpv_stream_cb_info;
class TorrentContent;

struct MpvTrack
{
    qint64 id = 0;
    QString type;
    QString title;
    QString language;
    bool selected = false;
    bool external = false;
};

class MpvVideoWidget final : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit MpvVideoWidget(QWidget *parent = nullptr);
    ~MpvVideoWidget() override;

    MpvVideoWidget(const MpvVideoWidget &) = delete;
    MpvVideoWidget &operator=(const MpvVideoWidget &) = delete;

    void loadFile(const QString &path);
    void setPaused(bool paused);
    void seekTo(double seconds);
    void setVolume(int percent);
    void selectAudioTrack(qint64 id);
    void selectSubtitleTrack(qint64 id);
    void addSubtitleFile(const QString &path);
    void setShaderFiles(const QStringList &paths);
    void setTorrentContent(const std::shared_ptr<TorrentContent> &content);

signals:
    void fatalError(const QString &message);
    void playbackError(const QString &message);
    void playbackRetrying(int attempt);
    void fileLoaded();
    void positionChanged(double seconds);
    void durationChanged(double seconds);
    void pauseChanged(bool paused);
    void tracksChanged(const QList<MpvTrack> &tracks);
    void toggleFullscreenRequested();

protected:
    void initializeGL() override;
    void paintGL() override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private slots:
    void drainEvents();

private:
    static void *resolveOpenGlSymbol(void *context, const char *name);
    static void handleMpvWakeup(void *context);
    static void handleRenderUpdate(void *context);
    static int openTorrentStream(
        void *context,
        char *uri,
        mpv_stream_cb_info *information
    );
    static std::int64_t readTorrentStream(
        void *stream,
        char *buffer,
        std::uint64_t byteCount
    );
    static std::int64_t seekTorrentStream(void *stream, std::int64_t offset);
    static std::int64_t sizeTorrentStream(void *stream);
    static void cancelTorrentStream(void *stream);
    static void closeTorrentStream(void *stream);

    bool initializeMpv();
    bool issueCommand(const QStringList &arguments);
    void loadCurrentFile();
    void processEvent(const mpv_event &event);
    void refreshTracks();
    void reportMpvError(const QString &operation, int errorCode);

    mpv_handle *m_mpv = nullptr;
    mpv_render_context *m_renderContext = nullptr;
    QString m_pendingFile;
    QString m_currentFile;
    int m_loadGeneration = 0;
    int m_torrentRetryCount = 0;
    bool m_currentFileLoaded = false;
    std::mutex m_torrentContentMutex;
    std::shared_ptr<TorrentContent> m_torrentContent;
};

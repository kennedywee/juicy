#pragma once

#include <QList>
#include <QOpenGLWidget>
#include <QString>

struct mpv_event;
struct mpv_handle;
struct mpv_render_context;

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

signals:
    void fatalError(const QString &message);
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

    bool initializeMpv();
    bool issueCommand(const QStringList &arguments);
    void processEvent(const mpv_event &event);
    void refreshTracks();
    void reportMpvError(const QString &operation, int errorCode);

    mpv_handle *m_mpv = nullptr;
    mpv_render_context *m_renderContext = nullptr;
    QString m_pendingFile;
};

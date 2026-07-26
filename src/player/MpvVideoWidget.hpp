#pragma once

#include <QOpenGLWidget>
#include <QString>

struct mpv_event;
struct mpv_handle;
struct mpv_render_context;

class MpvVideoWidget final : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit MpvVideoWidget(QWidget *parent = nullptr);
    ~MpvVideoWidget() override;

    MpvVideoWidget(const MpvVideoWidget &) = delete;
    MpvVideoWidget &operator=(const MpvVideoWidget &) = delete;

    void loadFile(const QString &path);

signals:
    void fatalError(const QString &message);
    void fileLoaded();

protected:
    void initializeGL() override;
    void paintGL() override;

private slots:
    void drainEvents();

private:
    static void *resolveOpenGlSymbol(void *context, const char *name);
    static void handleMpvWakeup(void *context);
    static void handleRenderUpdate(void *context);

    bool initializeMpv();
    bool issueCommand(const QStringList &arguments);
    void processEvent(const mpv_event &event);
    void reportMpvError(const QString &operation, int errorCode);

    mpv_handle *m_mpv = nullptr;
    mpv_render_context *m_renderContext = nullptr;
    QString m_pendingFile;
};


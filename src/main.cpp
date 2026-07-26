#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QMainWindow>
#include <QSurfaceFormat>
#include <QTimer>

#include "player/MpvVideoWidget.hpp"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QSurfaceFormat surfaceFormat;
    surfaceFormat.setRenderableType(QSurfaceFormat::OpenGL);
    surfaceFormat.setVersion(3, 3);
    surfaceFormat.setProfile(QSurfaceFormat::CoreProfile);
    surfaceFormat.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    QSurfaceFormat::setDefaultFormat(surfaceFormat);

    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Juicy"));
    QApplication::setOrganizationName(QStringLiteral("Juicy"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Minimal Linux torrent video player"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("video"), QStringLiteral("Local video used for player testing."));
    parser.process(application);

    QMainWindow window;
    auto *videoWidget = new MpvVideoWidget(&window);
    window.setCentralWidget(videoWidget);
    window.setWindowTitle(QStringLiteral("Juicy"));
    window.resize(960, 600);

    QObject::connect(videoWidget, &MpvVideoWidget::fatalError, [](const QString &message) {
        qCritical().noquote() << message;
    });
    QObject::connect(videoWidget, &MpvVideoWidget::fileLoaded, [] {
        qInfo() << "libmpv loaded the video";
    });

    window.show();

    const QStringList arguments = parser.positionalArguments();
    if (!arguments.isEmpty()) {
        videoWidget->loadFile(arguments.constFirst());
    }

    bool parsed = false;
    const int quitAfterMs = qEnvironmentVariableIntValue("JUICY_QUIT_AFTER_MS", &parsed);
    if (parsed && quitAfterMs >= 0) {
        QTimer::singleShot(quitAfterMs, &application, &QApplication::quit);
    }

    return application.exec();
}

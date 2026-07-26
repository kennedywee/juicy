#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QSurfaceFormat>
#include <QTimer>

#include "app/MainWindow.hpp"

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
    QCommandLineOption anime4kOption(
        QStringLiteral("anime4k"),
        QStringLiteral("Start with the Anime4K profile: off, fast, or quality."),
        QStringLiteral("profile"),
        QStringLiteral("off")
    );
    QCommandLineOption magnetOption(
        QStringLiteral("magnet"),
        QStringLiteral("Load a magnet link when the application starts."),
        QStringLiteral("uri")
    );
    parser.addOption(anime4kOption);
    parser.addOption(magnetOption);
    parser.addPositionalArgument(QStringLiteral("video"), QStringLiteral("Local video used for player testing."));
    parser.process(application);

    MainWindow window;
    window.setAnime4kProfile(parser.value(anime4kOption));
    QObject::connect(window.player(), &MpvVideoWidget::fatalError, [](const QString &message) {
        qCritical().noquote() << message;
    });
    QObject::connect(window.player(), &MpvVideoWidget::fileLoaded, [] {
        qInfo() << "libmpv loaded the video";
    });
    window.show();

    if (parser.isSet(magnetOption)) {
        window.openMagnet(parser.value(magnetOption));
    }

    const QStringList arguments = parser.positionalArguments();
    if (!arguments.isEmpty()) {
        window.openLocalFile(arguments.constFirst());
    }

    bool parsed = false;
    const int quitAfterMs = qEnvironmentVariableIntValue("JUICY_QUIT_AFTER_MS", &parsed);
    if (parsed && quitAfterMs >= 0) {
        QTimer::singleShot(quitAfterMs, &application, &QApplication::quit);
    }

    return application.exec();
}

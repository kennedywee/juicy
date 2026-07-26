#include <clocale>

#include <QApplication>
#include <QColor>
#include <QCommandLineParser>
#include <QPalette>
#include <QDebug>
#include <QFileInfo>
#include <QIcon>
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

    QApplication::setApplicationName(QStringLiteral("Juicy"));
    QApplication::setOrganizationName(QStringLiteral("Juicy"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QApplication application(argc, argv);
    // libmpv requires LC_NUMERIC to be "C"; QApplication resets it from the environment.
    std::setlocale(LC_NUMERIC, "C");
    QApplication::setStyle(QStringLiteral("fusion"));

    // Standard icons (play/pause, fullscreen) are painted from the palette, not
    // the stylesheet, so they need a dark palette to come out light.
    const QColor surface(0x28, 0x23, 0x20);
    const QColor sunken(0x21, 0x1d, 0x1a);
    const QColor foreground(0xd8, 0xd1, 0xc7);
    const QColor muted(0x6d, 0x66, 0x5e);
    QPalette palette;
    palette.setColor(QPalette::Window, surface);
    palette.setColor(QPalette::WindowText, foreground);
    palette.setColor(QPalette::Base, sunken);
    palette.setColor(QPalette::AlternateBase, surface);
    palette.setColor(QPalette::Text, foreground);
    palette.setColor(QPalette::Button, surface);
    palette.setColor(QPalette::ButtonText, foreground);
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::Light, foreground);
    palette.setColor(QPalette::Highlight, QColor(0x4c, 0x45, 0x3d));
    palette.setColor(QPalette::HighlightedText, foreground);
    palette.setColor(QPalette::ToolTipBase, sunken);
    palette.setColor(QPalette::ToolTipText, foreground);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, muted);
    palette.setColor(QPalette::Disabled, QPalette::Text, muted);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, muted);
    QApplication::setPalette(palette);

    application.setStyleSheet(QStringLiteral(R"(
        QWidget {
            color: #d8d1c7;
            font-family: "JetBrainsMono Nerd Font", "JetBrains Mono", "DejaVu Sans Mono", monospace;
            font-size: 13px;
        }
        QMainWindow, QStatusBar { background-color: #282320; }
        QStatusBar QLabel { color: #918a80; }
        QPushButton {
            background-color: transparent;
            border: 1px solid #d8d1c7;
            border-radius: 2px;
            padding: 4px 14px;
        }
        QPushButton:hover { background-color: #3b352f; }
        QPushButton:pressed { background-color: #4c453d; }
        QPushButton:disabled { color: #6d665e; border-color: #6d665e; }
        QLineEdit, QComboBox {
            background-color: #211d1a;
            border: 1px solid #6d665e;
            border-radius: 2px;
            padding: 4px 8px;
            selection-background-color: #4c453d;
        }
        QLineEdit:focus, QComboBox:focus { border-color: #d8d1c7; }
        QComboBox::drop-down { border: none; width: 18px; }
        QComboBox QAbstractItemView {
            background-color: #211d1a;
            border: 1px solid #6d665e;
            selection-background-color: #4c453d;
        }
        QSlider::groove:horizontal { height: 3px; background: #55504a; }
        QSlider::sub-page:horizontal { background: #d8d1c7; }
        QSlider::handle:horizontal {
            width: 10px;
            margin: -4px 0;
            background: #d8d1c7;
            border-radius: 2px;
        }
    )"));
    const QString sourceIcon = QStringLiteral(JUICY_SOURCE_ICON);
    if (QFileInfo::exists(sourceIcon)) {
        QApplication::setWindowIcon(QIcon(sourceIcon));
    } else {
        QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("juicy")));
    }

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
    QCommandLineOption contentFitOption(
        QStringLiteral("content-fit"),
        QStringLiteral("Start with the content fit: fit, cover, stretch, or original."),
        QStringLiteral("mode"),
        QStringLiteral("fit")
    );
    QCommandLineOption magnetOption(
        QStringLiteral("magnet"),
        QStringLiteral("Load a magnet link when the application starts."),
        QStringLiteral("uri")
    );
    QCommandLineOption autoStreamOption(
        QStringLiteral("auto-stream"),
        QStringLiteral("Automatically stream the first video after metadata loads.")
    );
    QCommandLineOption seekOption(
        QStringLiteral("seek-on-load"),
        QStringLiteral("Seek to a timestamp after the video loads."),
        QStringLiteral("seconds")
    );
    parser.addOption(anime4kOption);
    parser.addOption(contentFitOption);
    parser.addOption(magnetOption);
    parser.addOption(autoStreamOption);
    parser.addOption(seekOption);
    parser.addPositionalArgument(QStringLiteral("video"), QStringLiteral("Local video used for player testing."));
    parser.process(application);

    MainWindow window;
    window.setAutoStream(parser.isSet(autoStreamOption));
    window.setAnime4kProfile(parser.value(anime4kOption));
    window.setContentFit(parser.value(contentFitOption));
    QObject::connect(window.player(), &MpvVideoWidget::fatalError, [](const QString &message) {
        qCritical().noquote() << message;
    });
    QObject::connect(window.player(), &MpvVideoWidget::fileLoaded, [] {
        qInfo() << "libmpv loaded the video";
    });
    if (parser.isSet(seekOption)) {
        bool validSeek = false;
        const double seekSeconds = parser.value(seekOption).toDouble(&validSeek);
        if (validSeek && seekSeconds >= 0.0) {
            QObject::connect(
                window.player(),
                &MpvVideoWidget::fileLoaded,
                &window,
                [player = window.player(), seekSeconds] {
                    QTimer::singleShot(3000, player, [player, seekSeconds] {
                        player->seekTo(seekSeconds);
                    });
                }
            );
        }
    }
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

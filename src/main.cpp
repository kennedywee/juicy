#include <QApplication>
#include <QLabel>
#include <QMainWindow>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Juicy"));
    QApplication::setOrganizationName(QStringLiteral("Juicy"));

    QMainWindow window;
    auto *label = new QLabel(QStringLiteral("Juicy is ready for playback."), &window);
    label->setAlignment(Qt::AlignCenter);
    window.setCentralWidget(label);
    window.resize(960, 600);
    window.show();

    return application.exec();
}


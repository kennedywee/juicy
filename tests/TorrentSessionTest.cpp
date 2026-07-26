#include <QDir>
#include <QSignalSpy>
#include <QTest>

#include "torrent/TorrentSession.hpp"

class TorrentSessionTest final : public QObject
{
    Q_OBJECT

private slots:
    void rejectsInvalidMagnets();
    void acceptsWellFormedMagnets();
    void removesTemporaryStorage();
    void removesStaleTemporaryStorage();
};

void TorrentSessionTest::rejectsInvalidMagnets()
{
    TorrentSession session;
    QSignalSpy errors(&session, &TorrentSession::errorOccurred);

    QVERIFY(!session.addMagnet(QStringLiteral("this is not a magnet")));
    QCOMPARE(errors.count(), 1);
    QVERIFY(errors.constFirst().constFirst().toString().startsWith(
        QStringLiteral("Invalid magnet link:")
    ));
}

void TorrentSessionTest::acceptsWellFormedMagnets()
{
    TorrentSession session;
    const QString magnet = QStringLiteral(
        "magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567"
    );

    QVERIFY(session.addMagnet(magnet));
}

void TorrentSessionTest::removesTemporaryStorage()
{
    QString path;
    {
        TorrentSession session;
        path = session.temporaryPath();
        QVERIFY(QDir(path).exists());
    }
    QVERIFY(!QDir(path).exists());
}

void TorrentSessionTest::removesStaleTemporaryStorage()
{
    const QString stalePath = QDir(QDir::tempPath()).filePath(
        QStringLiteral("juicy-player-999999-ABC123")
    );
    QVERIFY(QDir().mkpath(stalePath));
    {
        TorrentSession session;
        QVERIFY(!QDir(stalePath).exists());
    }
}

QTEST_GUILESS_MAIN(TorrentSessionTest)

#include "TorrentSessionTest.moc"

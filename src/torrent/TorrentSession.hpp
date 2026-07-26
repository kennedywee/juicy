#pragma once

#include <memory>

#include <QList>
#include <QObject>
#include <QString>

class QTimer;
class TorrentContent;

struct TorrentFile
{
    int index = -1;
    QString name;
    QString path;
    qint64 size = 0;
};

class TorrentSession final : public QObject
{
    Q_OBJECT

public:
    explicit TorrentSession(QObject *parent = nullptr);
    ~TorrentSession() override;

    TorrentSession(const TorrentSession &) = delete;
    TorrentSession &operator=(const TorrentSession &) = delete;

    bool addMagnet(const QString &magnet);
    bool selectFile(int fileIndex);
    QString temporaryPath() const;

signals:
    void filesReady(const QList<TorrentFile> &files);
    void statusChanged(const QString &status);
    void errorOccurred(const QString &message);
    void fileSelected(const TorrentFile &file);
    void streamReady(
        const std::shared_ptr<TorrentContent> &content,
        const TorrentFile &file
    );

private slots:
    void processAlerts();
    void updateStatus();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    QTimer *m_alertTimer = nullptr;
    QTimer *m_statusTimer = nullptr;
};

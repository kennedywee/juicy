#include "app/DiagnosticsPanel.hpp"

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>

namespace {

constexpr int kPanelWidth = 258;
constexpr double kBytesPerMebibyte = 1024.0 * 1024.0;

QString mebibytes(double bytes)
{
    return QStringLiteral("%1 MiB").arg(bytes / kBytesPerMebibyte, 0, 'f', 1);
}

} // namespace

DiagnosticsPanel::DiagnosticsPanel(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("diagnosticsPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(kPanelWidth);
    setStyleSheet(QStringLiteral(
        "#diagnosticsPanel {"
        "  background-color: rgba(40, 35, 32, 235);"
        "  border: 1px solid #4c453d;"
        "  border-radius: 2px;"
        "}"
        "#diagnosticsTitle { color: #d8d1c7; letter-spacing: 2px; }"
        "#diagnosticsSection { color: #6d665e; letter-spacing: 1px; }"
        "#diagnosticsKey { color: #918a80; }"
        "#diagnosticsValue { color: #d8d1c7; }"
        "#diagnosticsFooter { color: #6d665e; }"
        "#diagnosticsClose {"
        "  border: none;"
        "  padding: 0px;"
        "  color: #918a80;"
        "  background: transparent;"
        "}"
        "#diagnosticsClose:hover { color: #d8d1c7; }"
    ));

    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(12, 10, 12, 10);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(3);
    grid->setColumnStretch(1, 1);

    auto *title = new QLabel(QStringLiteral("DIAGNOSTICS"), this);
    title->setObjectName(QStringLiteral("diagnosticsTitle"));
    auto *close = new QPushButton(QStringLiteral("×"), this);
    close->setObjectName(QStringLiteral("diagnosticsClose"));
    close->setFixedSize(18, 18);
    close->setToolTip(QStringLiteral("Close"));
    connect(close, &QPushButton::clicked, this, &DiagnosticsPanel::closeRequested);
    grid->addWidget(title, 0, 0);
    grid->addWidget(close, 0, 1, Qt::AlignRight);

    addSection(grid, QStringLiteral("PLAYBACK"));
    m_state = addRow(grid, QStringLiteral("State"));
    m_position = addRow(grid, QStringLiteral("Position"));

    addSection(grid, QStringLiteral("TORRENT"));
    m_torrentStatus = addRow(grid, QStringLiteral("Status"));
    m_rate = addRow(grid, QStringLiteral("Rate"));
    m_peers = addRow(grid, QStringLiteral("Peers"));
    m_downloaded = addRow(grid, QStringLiteral("Downloaded"));

    addSection(grid, QStringLiteral("ERRORS"));
    // Spans both columns: an error message is a sentence, not a value.
    m_error = new QLabel(QStringLiteral("—"), this);
    m_error->setObjectName(QStringLiteral("diagnosticsValue"));
    m_error->setWordWrap(true);
    grid->addWidget(m_error, grid->rowCount(), 0, 1, 2);

    auto *footer = new QLabel(QStringLiteral("Press D to toggle"), this);
    footer->setObjectName(QStringLiteral("diagnosticsFooter"));
    footer->setContentsMargins(0, 10, 0, 0);
    grid->addWidget(footer, grid->rowCount(), 0, 1, 2);
}

void DiagnosticsPanel::addSection(QGridLayout *grid, const QString &title)
{
    auto *label = new QLabel(title, this);
    label->setObjectName(QStringLiteral("diagnosticsSection"));
    label->setContentsMargins(0, 10, 0, 2);
    grid->addWidget(label, grid->rowCount(), 0, 1, 2);
}

QLabel *DiagnosticsPanel::addRow(QGridLayout *grid, const QString &name)
{
    const int row = grid->rowCount();
    auto *key = new QLabel(name, this);
    key->setObjectName(QStringLiteral("diagnosticsKey"));
    auto *value = new QLabel(QStringLiteral("—"), this);
    value->setObjectName(QStringLiteral("diagnosticsValue"));
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(key, row, 0);
    grid->addWidget(value, row, 1);
    return value;
}

void DiagnosticsPanel::setPlaybackState(const QString &state)
{
    m_state->setText(state);
}

void DiagnosticsPanel::setPosition(const QString &text)
{
    m_position->setText(text);
}

void DiagnosticsPanel::setTorrentStatus(const QString &status)
{
    m_torrentStatus->setText(status);
}

void DiagnosticsPanel::setTorrentStats(const TorrentStats &stats)
{
    m_rate->setText(
        QStringLiteral("%1/s").arg(mebibytes(static_cast<double>(stats.downloadRate)))
    );
    m_peers->setText(QString::number(stats.peers));
    m_downloaded->setText(mebibytes(static_cast<double>(stats.downloaded)));
}

void DiagnosticsPanel::setError(const QString &message)
{
    m_error->setText(message.isEmpty() ? QStringLiteral("—") : message);
}

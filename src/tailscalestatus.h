#pragma once

#include <QByteArray>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <optional>

enum class ConnectionState {
    Initializing, Connected, Disconnected, Starting, NeedsLogin, NeedsApproval, InUseOtherUser
};

struct TailDevice {
    QString key;
    QString name;
    QString ipv4;
    std::optional<bool> online;
};

struct TailscaleStatus {
    ConnectionState state = ConnectionState::Initializing;
    QString version;
    TailDevice self;
    QList<TailDevice> peers;
    QStringList health;
};
Q_DECLARE_METATYPE(TailscaleStatus)

struct StatusParseResult {
    std::optional<TailscaleStatus> status;
    QString error;
};

StatusParseResult parseTailscaleStatus(const QByteArray &json);
QString connectionLabel(ConnectionState state);
QString connectionGuidance(ConnectionState state);
// Names are untrusted menu text: do not interpret ampersands as accelerators.
QString menuText(const QString &text);

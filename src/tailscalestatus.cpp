#include "tailscalestatus.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <algorithm>

namespace {

bool absent(const QJsonValue &value)
{
    return value.isUndefined() || value.isNull();
}

bool stringField(const QJsonObject &object, const QString &key)
{
    return absent(object.value(key)) || object.value(key).isString();
}

bool readAddresses(const QJsonValue &value, QString &ipv4)
{
    if (absent(value)) {
        return true;
    }
    if (!value.isArray()) {
        return false;
    }
    for (const auto entry : value.toArray()) {
        if (!entry.isString()) {
            return false;
        }
        QHostAddress address;
        if (!address.setAddress(entry.toString())) {
            return false;
        }
        if (ipv4.isEmpty() && address.protocol() == QAbstractSocket::IPv4Protocol) {
            ipv4 = address.toString();
        }
    }
    return true;
}

bool readDevice(const QJsonObject &object, const QString &key, TailDevice &device)
{
    if (!stringField(object, "HostName") || !stringField(object, "DNSName")) {
        return false;
    }
    device.key = key;
    device.name = object.value("HostName").toString().trimmed();
    if (device.name.isEmpty()) {
        device.name = object.value("DNSName").toString().section('.', 0, 0).trimmed();
    }
    if (device.name.isEmpty()) {
        device.name = "Unnamed device";
    }
    const auto online = object.value("Online");
    if (!absent(online)) {
        if (!online.isBool()) {
            return false;
        }
        device.online = online.toBool();
    }
    // Never copy endpoint addresses, AllowedIPs routes, or IPv6 as IPv4.
    return readAddresses(object.value("TailscaleIPs"), device.ipv4);
}

int onlineRank(const TailDevice &device)
{
    return !device.online.has_value() ? 2 : (*device.online ? 0 : 1);
}

} // namespace

StatusParseResult parseTailscaleStatus(const QByteArray &json)
{
    const auto malformed = [] {
        return StatusParseResult{std::nullopt,
            "Tailscale returned an unsupported status structure. Check its version and retry."};
    };
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        // Do not surface parser snippets: output may contain credentials or identifiers.
        return {std::nullopt, "Tailscale returned invalid JSON status. Retry or check the CLI installation."};
    }
    const auto object = document.object();
    if (!object.value("BackendState").isString()) {
        return malformed();
    }

    TailscaleStatus status;
    const auto backend = object.value("BackendState").toString();
    if (backend == "Running") status.state = ConnectionState::Connected;
    else if (backend == "Stopped") status.state = ConnectionState::Disconnected;
    else if (backend == "Starting") status.state = ConnectionState::Starting;
    else if (backend == "NoState") status.state = ConnectionState::Initializing;
    else if (backend == "NeedsLogin") status.state = ConnectionState::NeedsLogin;
    else if (backend == "NeedsMachineAuth") status.state = ConnectionState::NeedsApproval;
    else if (backend == "InUseOtherUser") status.state = ConnectionState::InUseOtherUser;
    else return {std::nullopt, "This Tailscale backend state is not supported. Update TailSwitch or check Tailscale manually."};

    if (!stringField(object, "Version")) {
        return malformed();
    }
    status.version = object.value("Version").toString();
    const auto self = object.value("Self");
    if (!absent(self) && !self.isObject()) {
        return malformed();
    }
    if (!readDevice(self.toObject(), "self", status.self)) {
        return malformed();
    }
    if (status.self.name == "Unnamed device") {
        status.self.name = "This device";
    }
    // Some non-running states omit Self but retain the top-level address list.
    QString localAddress;
    if (!readAddresses(object.value("TailscaleIPs"), localAddress)) {
        return malformed();
    }
    if (status.self.ipv4.isEmpty()) {
        status.self.ipv4 = localAddress;
    }

    const auto peers = object.value("Peer");
    if (!absent(peers) && !peers.isObject()) {
        return malformed();
    }
    const auto peerObject = peers.toObject();
    for (auto it = peerObject.begin(); it != peerObject.end(); ++it) {
        if (!it.value().isObject()) {
            return malformed();
        }
        TailDevice device;
        if (!readDevice(it.value().toObject(), it.key(), device)) {
            return malformed();
        }
        status.peers.append(device);
    }
    std::sort(status.peers.begin(), status.peers.end(), [](const auto &a, const auto &b) {
        if (onlineRank(a) != onlineRank(b)) return onlineRank(a) < onlineRank(b);
        const int nameOrder = QString::compare(a.name, b.name, Qt::CaseInsensitive);
        if (nameOrder != 0) return nameOrder < 0;
        if (a.ipv4 != b.ipv4) return a.ipv4 < b.ipv4;
        return a.key < b.key;
    });

    const auto health = object.value("Health");
    if (!absent(health)) {
        if (!health.isArray()) return malformed();
        for (const auto entry : health.toArray()) {
            if (!entry.isString()) return malformed();
            if (!entry.toString().isEmpty()) status.health.append(entry.toString());
        }
    }
    return {status, {}};
}

QString connectionLabel(ConnectionState state)
{
    switch (state) {
    case ConnectionState::Connected: return "Connected";
    case ConnectionState::Disconnected: return "Disconnected";
    case ConnectionState::Starting: return "Connecting";
    case ConnectionState::NeedsLogin: return "Sign-in required";
    case ConnectionState::NeedsApproval: return "Device approval required";
    case ConnectionState::InUseOtherUser: return "In use by another user";
    case ConnectionState::Initializing: return "Initializing";
    }
    return "Unknown";
}

QString connectionGuidance(ConnectionState state)
{
    switch (state) {
    case ConnectionState::Connected: return "Tailscale reports that it is connected. This is not an end-to-end connectivity test.";
    case ConnectionState::Disconnected: return "Tailscale is disconnected. Network controls are not implemented in this read-only version; use the CLI to reconnect.";
    case ConnectionState::Starting: return "Tailscale is starting. Status will refresh automatically.";
    case ConnectionState::NeedsLogin: return "Tailscale needs an existing login to be renewed or configured. Complete authentication using the CLI, then refresh. TailSwitch does not initiate login.";
    case ConnectionState::NeedsApproval: return "This device needs approval from a tailnet administrator. Approve it in the Tailscale admin console, then refresh.";
    case ConnectionState::InUseOtherUser: return "Tailscale reports that another local user is using it. Check the active Tailscale user/profile manually.";
    case ConnectionState::Initializing: return "Tailscale has not finished initializing. If this persists, check the tailscaled service.";
    }
    return {};
}

QString menuText(const QString &text)
{
    QString result = text.left(160);
    for (auto &character : result) {
        if (character.category() == QChar::Other_Control
            || character.category() == QChar::Separator_Line
            || character.category() == QChar::Separator_Paragraph) {
            character = QLatin1Char(' ');
        }
    }
    return result.replace('&', "&&");
}

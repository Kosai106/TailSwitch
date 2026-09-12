#include "kdeclipboard.h"

#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

KdeClipboard::KdeClipboard(QObject *parent, const QDBusConnection &bus,
                           const QString &service)
    : QObject(parent), m_bus(bus), m_service(service)
{
}

void KdeClipboard::copyText(const QString &text)
{
    if (text.isEmpty()) {
        emit copyFailed(QStringLiteral("No address is available to copy."));
        return;
    }

    auto request = QDBusMessage::createMethodCall(
        m_service, QStringLiteral("/klipper"),
        QStringLiteral("org.kde.klipper.klipper"), QStringLiteral("setClipboardContents"));
    request.setAutoStartService(false);
    request << text;

    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(request, 3000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher] {
        const QDBusPendingReply<> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError()) {
            emit copyFailed(QStringLiteral(
                "KDE's clipboard service could not accept the copy request. "
                "Check that Plasma's Clipboard manager (Klipper) is running.\n%1")
                .arg(reply.error().message()));
            return;
        }
        // Service acknowledgement, not an independent paste verification.
        // Do not read or log the user's clipboard/history to verify writes.
        emit copySucceeded();
    });
}

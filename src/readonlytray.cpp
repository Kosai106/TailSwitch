#include "readonlytray.h"

#include "appicon.h"
#include "kdeclipboard.h"
#include "traymenu.h"

#include <KStatusNotifierItem>
#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QMenu>
#include <QMessageBox>
#include <QSet>

namespace {

QString deviceLabel(const TailDevice &device, bool peer)
{
    QString label = menuText(device.name) + " · "
        + (device.ipv4.isEmpty() ? QString("No IPv4") : device.ipv4);
    if (peer) {
        if (!device.online.has_value()) label += " · Status unknown";
        else if (!*device.online) label += " · Offline";
    }
    return label;
}

bool needsAttention(ConnectionState state)
{
    return state == ConnectionState::NeedsLogin || state == ConnectionState::NeedsApproval
        || state == ConnectionState::InUseOtherUser;
}

} // namespace

ReadOnlyTray::ReadOnlyTray(KStatusNotifierItem &tray, TailscaleClient &client,
                           KdeClipboard &clipboard, QObject *parent)
    : QObject(parent), m_tray(tray), m_clipboard(clipboard), m_menu(new QMenu)
{
    configureTrayMenu(m_tray, *m_menu); // The native item owns this heap menu.
    m_header = m_menu->addAction("TailSwitch · Reading status…");
    m_header->setObjectName("connectionStatus");
    m_header->setEnabled(false);
    m_menu->addAction("Read-only · Network controls coming next")->setEnabled(false);
    m_menu->addSeparator();
    auto *selfMenu = m_menu->addMenu("This device");
    selfMenu->setObjectName("selfMenu");
    m_self = selfMenu->addAction("Waiting for status…");
    m_self->setEnabled(false);
    connect(m_self, &QAction::triggered, this, [this] {
        requestCopy(m_self->data().toString());
    });
    m_devices = m_menu->addMenu("Devices");
    m_devices->setObjectName("devicesMenu");
    m_empty = m_devices->addAction("Waiting for status…");
    m_empty->setEnabled(false);
    m_copyFeedback = m_menu->addAction("Click a device to copy its IPv4");
    m_copyFeedback->setEnabled(false);
    m_menu->addSeparator();
    m_refresh = m_menu->addAction("Refresh now");
    connect(m_refresh, &QAction::triggered, &client, &TailscaleClient::refresh);
    connect(&client, &TailscaleClient::busyChanged, this, [this](bool busy) {
        m_refresh->setEnabled(!busy);
        m_refresh->setText(busy ? "Refreshing…" : "Refresh now");
    });
    connect(m_menu->addAction("Status details…"), &QAction::triggered, this, &ReadOnlyTray::showDetails);
    connect(m_menu->addAction("About…"), &QAction::triggered, this, [this] {
        auto *dialog = new QMessageBox(QMessageBox::Information, "About TailSwitch",
            "TailSwitch — read-only preview\n\n"
            "An unofficial client, not affiliated with or endorsed by Tailscale.\n"
            "Displays local CLI status and copies Tailscale IPv4 addresses using KDE Clipboard.\n"
            "It does not change Tailscale settings or initiate login.", QMessageBox::Ok);
        dialog->setTextFormat(Qt::PlainText);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(this, &QObject::destroyed, dialog, &QWidget::close);
        dialog->show();
    });
    connect(m_menu->addAction("Quit"), &QAction::triggered, qApp, &QApplication::quit);

    m_copyReset.setSingleShot(true);
    m_copyReset.setInterval(4000);
    connect(&m_copyReset, &QTimer::timeout, this, [this] {
        m_copyFeedback->setText("Click a device to copy its IPv4");
    });
    connect(&clipboard, &KdeClipboard::copySucceeded, this, [this] {
        m_copyBusy = false;
        updateCopyActions();
        m_copyFeedback->setText("IPv4 copied via KDE Clipboard");
        m_copyReset.start();
    });
    connect(&clipboard, &KdeClipboard::copyFailed, this, [this](const QString &message) {
        m_copyBusy = false;
        updateCopyActions();
        m_copyFeedback->setText("Copy failed — check Plasma's Clipboard manager");
        m_tray.showMessage("TailSwitch — copy failed", message, "dialog-warning");
    });
    connect(&client, &TailscaleClient::statusReceived, this, &ReadOnlyTray::applyStatus);
    connect(&client, &TailscaleClient::failed, this, &ReadOnlyTray::applyFailure);

    const QIcon icon = tailscaleLogoIcon();
    m_tray.setTitle("TailSwitch");
    m_tray.setCategory(KStatusNotifierItem::Communications);
    m_tray.setIconByPixmap(icon);
    m_tray.setAttentionIconByPixmap(icon);
    m_tray.setToolTipIconByPixmap(icon);
    m_tray.setToolTipTitle("TailSwitch — reading status");
    m_tray.setStatus(KStatusNotifierItem::Active);
    updateDetails("Waiting for the first status read. No networking settings are changed by this app.");
}

ReadOnlyTray::~ReadOnlyTray()
{
    delete m_details.data();
}

void ReadOnlyTray::syncDevices(const QList<TailDevice> &devices)
{
    QSet<QString> wanted;
    QStringList order;
    for (const auto &device : devices) {
        wanted.insert(device.key);
        order.append(device.key);
        auto *action = m_peers.value(device.key, nullptr);
        if (!action) {
            action = new QAction(m_devices);
            m_peers.insert(device.key, action);
            connect(action, &QAction::triggered, this, [this, action] {
                requestCopy(action->data().toString());
            });
        }
        action->setText(deviceLabel(device, true));
        action->setData(device.ipv4);
    }
    for (auto it = m_peers.begin(); it != m_peers.end();) {
        if (!wanted.contains(it.key())) {
            it.value()->setEnabled(false);
            it.value()->setData(QString{});
            m_devices->removeAction(it.value());
            it.value()->deleteLater();
            it = m_peers.erase(it);
        } else {
            ++it;
        }
    }
    // Reuse QActions between refreshes; do not clear/rebuild an open exported
    // menu every ten seconds. Only reinsert when membership/order changes.
    if (order != m_order) {
        for (auto *action : m_peers) m_devices->removeAction(action);
        for (const auto &key : order) m_devices->addAction(m_peers.value(key));
        m_order = order;
    }
    m_empty->setVisible(devices.isEmpty());
    m_devices->setTitle(QString("Devices (%1)").arg(devices.size()));
    updateCopyActions();
}

void ReadOnlyTray::applyStatus(const TailscaleStatus &status)
{
    m_hasStatus = true;
    m_lastFailure.reset();
    // Health messages remain visible in the header/details, but do not pulse
    // the tray icon. Reserve NeedsAttention for states requiring user action.
    const bool attention = needsAttention(status.state);
    const QString label = connectionLabel(status.state)
        + (!status.health.isEmpty() ? QString(" · Health warning") : QString{});
    m_header->setText("TailSwitch · " + label);
    m_tray.setToolTipTitle("TailSwitch — " + label);
    m_tray.setStatus(attention ? KStatusNotifierItem::NeedsAttention : KStatusNotifierItem::Active);
    m_self->setText(deviceLabel(status.self, false));
    m_self->setData(status.self.ipv4);
    m_empty->setText("No devices reported");
    syncDevices(status.peers);

    QString details = connectionLabel(status.state) + "\n\n" + connectionGuidance(status.state)
        + QString("\n\nCLI version: %1\nDevices reported: %2\nLast successful read: %3")
              .arg(status.version.isEmpty() ? "Not reported" : status.version.left(100))
              .arg(status.peers.size())
              .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    if (!status.health.isEmpty()) {
        // Health text is only displayed locally, as plain text; never logged.
        details += "\n\nTailscale health messages:\n" + status.health.join("\n\n").left(16000);
    }
    updateDetails(details);
}

void ReadOnlyTray::applyFailure(const StatusFailure &failure)
{
    m_hasStatus = false;
    m_header->setText("TailSwitch · Status unavailable");
    m_tray.setToolTipTitle("TailSwitch — status unavailable");
    m_tray.setStatus(KStatusNotifierItem::NeedsAttention);
    m_self->setText("Status unavailable");
    m_self->setData(QString{});
    m_empty->setText("Status unavailable — see Status details");
    syncDevices({});
    updateDetails(failure.message + "\n\nDevice actions are unavailable until a successful refresh. "
                  "TailSwitch has not changed any Tailscale settings. Automatic retries continue.");
    if (!m_lastFailure || m_lastFailure->code != failure.code
        || m_lastFailure->message != failure.message) {
        m_tray.showMessage("TailSwitch — status unavailable", failure.message, "dialog-warning");
    }
    m_lastFailure = failure;
}

void ReadOnlyTray::requestCopy(const QString &address)
{
    if (!m_hasStatus || m_copyBusy || address.isEmpty()) return;
    m_copyReset.stop();
    m_copyBusy = true;
    updateCopyActions();
    m_copyFeedback->setText("Copying IPv4…");
    m_clipboard.copyText(address);
}

void ReadOnlyTray::updateCopyActions()
{
    const bool enabled = m_hasStatus && !m_copyBusy;
    m_self->setEnabled(enabled && !m_self->data().toString().isEmpty());
    for (auto *action : m_peers) {
        action->setEnabled(enabled && !action->data().toString().isEmpty());
    }
}

void ReadOnlyTray::showDetails()
{
    if (!m_details) {
        m_details = new QMessageBox(QMessageBox::Information, "TailSwitch — status details",
                                   m_detailsText, QMessageBox::Ok);
        m_details->setTextFormat(Qt::PlainText);
        m_details->setAttribute(Qt::WA_DeleteOnClose);
    }
    m_details->show();
    m_details->raise();
    m_details->activateWindow();
}

void ReadOnlyTray::updateDetails(const QString &text)
{
    m_detailsText = text;
    if (m_details) m_details->setText(text);
}

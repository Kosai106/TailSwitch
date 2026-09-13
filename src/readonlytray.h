#pragma once

#include "tailscaleclient.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTimer>

class Autostart;
class QAction;
class KdeClipboard;
class KStatusNotifierItem;
class QMenu;
class QMessageBox;

class ReadOnlyTray final : public QObject
{
    Q_OBJECT
public:
    // autostart may be null when launch-at-login is not offered (tests).
    ReadOnlyTray(KStatusNotifierItem &tray, TailscaleClient &client,
                 KdeClipboard &clipboard, Autostart *autostart = nullptr,
                 QObject *parent = nullptr);
    ~ReadOnlyTray() override;

public slots:
    void applyStatus(const TailscaleStatus &status);
    void applyFailure(const StatusFailure &failure);

private:
    void syncDevices(const QList<TailDevice> &devices);
    void requestCopy(const QString &address);
    void updateCopyActions();
    void showDetails();
    void updateDetails(const QString &text);
    void showAbout();
    void toggleAutostart(bool enabled);

    KStatusNotifierItem &m_tray;
    KdeClipboard &m_clipboard;
    Autostart *m_autostart;
    QMenu *m_menu;
    QMenu *m_devices;
    QAction *m_header;
    QAction *m_self;
    QAction *m_empty;
    QAction *m_copyFeedback;
    QAction *m_refresh;
    QAction *m_autostartAction = nullptr;
    QHash<QString, QAction *> m_peers;
    QStringList m_order;
    QString m_detailsText;
    QPointer<QMessageBox> m_details;
    QTimer m_copyReset;
    bool m_copyBusy = false;
    bool m_hasStatus = false;
    std::optional<StatusFailure> m_lastFailure;
};

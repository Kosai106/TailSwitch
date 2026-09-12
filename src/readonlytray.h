#pragma once

#include "tailscaleclient.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTimer>

class QAction;
class KdeClipboard;
class KStatusNotifierItem;
class QMenu;
class QMessageBox;

class ReadOnlyTray final : public QObject
{
    Q_OBJECT
public:
    ReadOnlyTray(KStatusNotifierItem &tray, TailscaleClient &client,
                 KdeClipboard &clipboard, QObject *parent = nullptr);
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

    KStatusNotifierItem &m_tray;
    KdeClipboard &m_clipboard;
    QMenu *m_menu;
    QMenu *m_devices;
    QAction *m_header;
    QAction *m_self;
    QAction *m_empty;
    QAction *m_copyFeedback;
    QAction *m_refresh;
    QHash<QString, QAction *> m_peers;
    QStringList m_order;
    QString m_detailsText;
    QPointer<QMessageBox> m_details;
    QTimer m_copyReset;
    bool m_copyBusy = false;
    bool m_hasStatus = false;
    std::optional<StatusFailure> m_lastFailure;
};

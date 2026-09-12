#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QString>

// Plasma's exported tray menus run in plasmashell, so clicking one need not
// give this process a Wayland input serial. Ask Klipper to own the selection
// instead of using QClipboard::setText from an unfocused application.
class KdeClipboard final : public QObject
{
    Q_OBJECT

public:
    explicit KdeClipboard(QObject *parent = nullptr,
                          const QDBusConnection &bus = QDBusConnection::sessionBus(),
                          const QString &service = QStringLiteral("org.kde.klipper"));

    void copyText(const QString &text);

signals:
    void copySucceeded();
    void copyFailed(const QString &message);

private:
    QDBusConnection m_bus;
    QString m_service;
};

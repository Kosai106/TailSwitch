#include "kdeclipboard.h"
#include "traymenu.h"

#include <QDBusContext>
#include <QDBusError>
#include <QMenu>
#include <QSignalSpy>
#include <QSystemTrayIcon>
#include <QtTest>

class FakeKlipper final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.klipper.klipper")

public:
    QString lastText;
    int calls = 0;
    bool deny = false;

public slots:
    void setClipboardContents(const QString &text)
    {
        ++calls;
        if (deny) {
            sendErrorReply(QDBusError::AccessDenied, "Synthetic access denial");
            return;
        }
        lastText = text;
    }
};

class DesktopTest final : public QObject
{
    Q_OBJECT

private slots:
    void primaryClickOpensMenu()
    {
        QSystemTrayIcon tray;
        QMenu menu;
        menu.addAction("Synthetic device");
        configureTrayMenu(tray, menu);
        QCOMPARE(tray.contextMenu(), &menu);
        QVERIFY(!menu.isVisible());
        tray.activated(QSystemTrayIcon::Trigger);
        QVERIFY(menu.isVisible());
        // Repeated activation must not close an already visible menu.
        tray.activated(QSystemTrayIcon::Trigger);
        QVERIFY(menu.isVisible());
        menu.hide();
        tray.activated(QSystemTrayIcon::Trigger);
        QVERIFY(menu.isVisible());
    }

    void otherActivationsLeaveContextMenuToPlatform()
    {
        QSystemTrayIcon tray;
        QMenu menu;
        menu.addAction("Synthetic device");
        configureTrayMenu(tray, menu);
        for (const auto reason : {QSystemTrayIcon::Context, QSystemTrayIcon::MiddleClick,
                                  QSystemTrayIcon::DoubleClick, QSystemTrayIcon::Unknown}) {
            tray.activated(reason);
            QVERIFY(!menu.isVisible());
        }
        QCOMPARE(tray.contextMenu(), &menu);
    }

    void copyThroughService()
    {
        FakeKlipper service;
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.registerService("org.tailswitch.TestClipboard"));
        QVERIFY(bus.registerObject("/klipper", &service, QDBusConnection::ExportAllSlots));
        KdeClipboard clipboard(nullptr, bus, "org.tailswitch.TestClipboard");
        QSignalSpy success(&clipboard, &KdeClipboard::copySucceeded);
        QSignalSpy failure(&clipboard, &KdeClipboard::copyFailed);

        clipboard.copyText("100.64.0.1");
        QTRY_COMPARE(success.count(), 1);
        QCOMPARE(failure.count(), 0);
        QCOMPARE(service.calls, 1);
        QCOMPARE(service.lastText, QString("100.64.0.1"));

        bus.unregisterObject("/klipper");
        QVERIFY(bus.unregisterService("org.tailswitch.TestClipboard"));
    }

    void serviceFailureDoesNotReportSuccess()
    {
        FakeKlipper service;
        service.deny = true;
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.registerService("org.tailswitch.TestClipboard"));
        QVERIFY(bus.registerObject("/klipper", &service, QDBusConnection::ExportAllSlots));
        KdeClipboard clipboard(nullptr, bus, "org.tailswitch.TestClipboard");
        QSignalSpy success(&clipboard, &KdeClipboard::copySucceeded);
        QSignalSpy failure(&clipboard, &KdeClipboard::copyFailed);

        clipboard.copyText("100.64.0.1");
        QTRY_COMPARE(failure.count(), 1);
        QCOMPARE(success.count(), 0);
        QVERIFY(failure.at(0).at(0).toString().contains("Synthetic access denial"));
        QVERIFY(service.lastText.isEmpty());

        bus.unregisterObject("/klipper");
        QVERIFY(bus.unregisterService("org.tailswitch.TestClipboard"));
    }

    void missingServiceDoesNotReportSuccess()
    {
        KdeClipboard clipboard(nullptr, QDBusConnection::sessionBus(),
                               "org.tailswitch.MissingClipboard");
        QSignalSpy success(&clipboard, &KdeClipboard::copySucceeded);
        QSignalSpy failure(&clipboard, &KdeClipboard::copyFailed);
        clipboard.copyText("100.64.0.1");
        QTRY_COMPARE(failure.count(), 1);
        QCOMPARE(success.count(), 0);
        QVERIFY(failure.at(0).at(0).toString().contains("Klipper"));
    }

    void emptyTextIsRejected()
    {
        KdeClipboard clipboard(nullptr, QDBusConnection::sessionBus(),
                               "org.tailswitch.MissingClipboard");
        QSignalSpy success(&clipboard, &KdeClipboard::copySucceeded);
        QSignalSpy failure(&clipboard, &KdeClipboard::copyFailed);
        clipboard.copyText({});
        QCOMPARE(failure.count(), 1);
        QCOMPARE(success.count(), 0);
        QCOMPARE(failure.at(0).at(0).toString(), QString("No address is available to copy."));
    }
};

QTEST_MAIN(DesktopTest)
#include "test_desktop.moc"

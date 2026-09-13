#include "appicon.h"
#include "autostart.h"
#include "kdeclipboard.h"
#include "readonlytray.h"
#include "traymenu.h"

#include <KStatusNotifierItem>
#include <QDBusContext>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QMenu>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
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

class FakeTrayWatcher final : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierWatcher")
    Q_PROPERTY(bool IsStatusNotifierHostRegistered READ hostRegistered)
    Q_PROPERTY(int ProtocolVersion READ protocolVersion)

public:
    QString registeredService;
    QString registeredPath;
    bool hostRegistered() const { return true; }
    int protocolVersion() const { return 0; }

public slots:
    void RegisterStatusNotifierItem(const QString &serviceOrPath)
    {
        registeredService = serviceOrPath.startsWith('/') ? message().service() : serviceOrPath;
        registeredPath = serviceOrPath.startsWith('/') ? serviceOrPath : "/StatusNotifierItem";
    }
};

class FakeNotifications final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
public:
    uint calls = 0;
public slots:
    QStringList GetCapabilities() { return {"body"}; }
    uint Notify(const QString &, uint, const QString &, const QString &, const QString &,
                const QStringList &, const QVariantMap &, int)
    {
        return ++calls;
    }
};

class DesktopTest final : public QObject
{
    Q_OBJECT

private:
    FakeTrayWatcher m_watcher;
    FakeNotifications m_notifications;

private slots:
    void initTestCase()
    {
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.registerService("org.kde.StatusNotifierWatcher"));
        QVERIFY(bus.registerObject("/StatusNotifierWatcher", &m_watcher,
                                  QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllProperties));
        QVERIFY(bus.registerService("org.freedesktop.Notifications"));
        QVERIFY(bus.registerObject("/org/freedesktop/Notifications", &m_notifications,
                                  QDBusConnection::ExportAllSlots));
    }

    void applicationIconIsEmbeddedAndCropped()
    {
        const QIcon icon = tailscaleLogoIcon();
        QVERIFY(!icon.isNull());
        const QImage image = icon.pixmap(64, 64).toImage();
        QCOMPARE(image.size(), QSize(64, 64));
        QRect opaqueBounds;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if (qAlpha(image.pixel(x, y)) > 0) {
                    opaqueBounds |= QRect(x, y, 1, 1);
                }
            }
        }
        QVERIFY(!opaqueBounds.isEmpty());
        // The original 130x120 canvas rendered the 53-unit mark at roughly
        // half this width. Keep a deliberate but compact tray-safe margin.
        QVERIFY(opaqueBounds.width() >= 51);
        QVERIFY(opaqueBounds.height() >= 51);
        QVERIFY(opaqueBounds.left() >= 4);
        QVERIFY(opaqueBounds.top() >= 4);
        QVERIFY(opaqueBounds.right() <= 59);
        QVERIFY(opaqueBounds.bottom() <= 59);
    }

    void nativeMenuIsExported()
    {
        KStatusNotifierItem tray(QStringLiteral("tailswitch-test"));
        auto *menu = new QMenu;
        menu->addAction("Synthetic device");
        configureTrayMenu(tray, *menu);
        tray.setStatus(KStatusNotifierItem::Active);
        QCOMPARE(tray.contextMenu(), menu);
        QVERIFY(tray.isMenu());
        QVERIFY(!tray.standardActionsEnabled());

        // Verify the actual D-Bus contract consumed by Plasma, not merely
        // QMenu visibility in an offscreen platform without Wayland grabs.
        QTRY_VERIFY(!m_watcher.registeredService.isEmpty());
        auto bus = QDBusConnection::sessionBus();
        auto request = QDBusMessage::createMethodCall(
            m_watcher.registeredService, m_watcher.registeredPath,
            "org.freedesktop.DBus.Properties", "Get");
        request << "org.kde.StatusNotifierItem" << "ItemIsMenu";
        QDBusPendingCallWatcher menuOnlyCall(bus.asyncCall(request, 2000));
        QTRY_VERIFY(menuOnlyCall.isFinished());
        QDBusPendingReply<QDBusVariant> menuOnlyReply = menuOnlyCall;
        QVERIFY2(!menuOnlyReply.isError(), qPrintable(menuOnlyReply.error().message()));
        QVERIFY(menuOnlyReply.value().variant().toBool());

        request.setArguments({"org.kde.StatusNotifierItem", "Menu"});
        QDBusPendingCallWatcher menuPathCall(bus.asyncCall(request, 2000));
        QTRY_VERIFY(menuPathCall.isFinished());
        QDBusPendingReply<QDBusVariant> menuPathReply = menuPathCall;
        QVERIFY2(!menuPathReply.isError(), qPrintable(menuPathReply.error().message()));
        const QString path = menuPathReply.value().variant().value<QDBusObjectPath>().path();
        QVERIFY(!path.isEmpty());
        QVERIFY(path != "/");

        auto introspect = QDBusMessage::createMethodCall(
            m_watcher.registeredService, path, "org.freedesktop.DBus.Introspectable", "Introspect");
        QDBusPendingCallWatcher introspectCall(bus.asyncCall(introspect, 2000));
        QTRY_VERIFY(introspectCall.isFinished());
        QDBusPendingReply<QString> introspectReply = introspectCall;
        QVERIFY2(!introspectReply.isError(), qPrintable(introspectReply.error().message()));
        QVERIFY(introspectReply.value().contains("com.canonical.dbusmenu"));
        QVERIFY(!menu->isVisible());
    }

    void menuOwnershipAndNoLocalPopup()
    {
        QPointer<QMenu> ownedMenu;
        {
            KStatusNotifierItem tray(QStringLiteral("tailswitch-ownership-test"));
            auto *menu = new QMenu;
            ownedMenu = menu;
            menu->addAction("Synthetic device");
            configureTrayMenu(tray, *menu);
            tray.activateRequested(true, QPoint(10, 10));
            tray.secondaryActivateRequested(QPoint(10, 10));
            QVERIFY(!menu->isVisible());
            QCOMPARE(tray.contextMenu(), menu);
        }
        QVERIFY(ownedMenu.isNull());
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

    void readOnlyMenuTracksStatusAndClipboard()
    {
        FakeKlipper service;
        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.registerService("org.tailswitch.TestClipboard"));
        QVERIFY(bus.registerObject("/klipper", &service, QDBusConnection::ExportAllSlots));
        KdeClipboard clipboard(nullptr, bus, "org.tailswitch.TestClipboard");
        TailscaleClient client; // Never started; no real CLI is queried in this test.
        KStatusNotifierItem tray(QStringLiteral("tailswitch-readonly-test"));
        ReadOnlyTray controller(tray, client, clipboard);
        QFile fixture(QStringLiteral(TAILSWITCH_FIXTURE_DIR) + "/status-running.json");
        QVERIFY(fixture.open(QIODevice::ReadOnly));
        auto parsed = parseTailscaleStatus(fixture.readAll());
        QVERIFY(parsed.status.has_value());
        auto status = *parsed.status;
        controller.applyStatus(status);
        // The synthetic fixture has a health message. It stays visible in
        // details/header but must not make Plasma pulse the icon.
        QCOMPARE(tray.status(), KStatusNotifierItem::Active);
        QVERIFY(!tray.iconPixmap().isNull());

        auto *devices = tray.contextMenu()->findChild<QMenu *>("devicesMenu");
        auto *self = tray.contextMenu()->findChild<QMenu *>("selfMenu");
        QVERIFY(devices);
        QVERIFY(self);
        QList<QAction *> visible;
        for (auto *action : devices->actions()) {
            if (action->isVisible()) visible.append(action);
        }
        QCOMPARE(visible.size(), 5);
        QVERIFY(visible[0]->text().contains("alpha && dev"));
        QVERIFY(visible[2]->text().contains("Offline"));
        QVERIFY(visible[2]->isEnabled());
        QVERIFY(!visible[3]->isEnabled());
        QVERIFY(!visible[4]->isEnabled());
        QCOMPARE(self->actions()[0]->data().toString(), QString("100.64.0.1"));

        QSignalSpy success(&clipboard, &KdeClipboard::copySucceeded);
        auto *offline = visible[2];
        offline->trigger();
        QVERIFY(!offline->isEnabled());
        QTRY_COMPARE(success.count(), 1);
        QCOMPARE(service.lastText, QString("100.64.0.4"));
        QVERIFY(offline->isEnabled());

        status.peers[2].ipv4 = "100.64.0.44";
        controller.applyStatus(status);
        QVERIFY(devices->actions().contains(offline)); // Reused, not rebuilt.
        QCOMPARE(offline->data().toString(), QString("100.64.0.44"));
        offline->trigger();
        QTRY_COMPARE(success.count(), 2);
        QCOMPARE(service.lastText, QString("100.64.0.44"));

        const auto notificationsBefore = m_notifications.calls;
        const StatusFailure failure{StatusError::TimedOut, "Synthetic timeout"};
        controller.applyFailure(failure);
        QCOMPARE(tray.status(), KStatusNotifierItem::NeedsAttention);
        QVERIFY(!self->actions()[0]->isEnabled());
        QVERIFY(!offline->isEnabled());
        QVERIFY(offline->data().toString().isEmpty());
        offline->trigger();
        QTRY_COMPARE(m_notifications.calls, notificationsBefore + 1);
        QCOMPARE(success.count(), 2);
        for (auto *action : devices->actions()) QVERIFY(!action->isEnabled());
        controller.applyFailure(failure);
        QTest::qWait(50);
        QCOMPARE(m_notifications.calls, notificationsBefore + 1);

        controller.applyStatus(status);
        QVERIFY(self->actions()[0]->isEnabled());
        controller.applyFailure(failure);
        QTRY_COMPARE(m_notifications.calls, notificationsBefore + 2);
        bus.unregisterObject("/klipper");
        QVERIFY(bus.unregisterService("org.tailswitch.TestClipboard"));
    }

    void autostartEntryLifecycle()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString autostartDir = dir.path() + "/config/autostart";
        Autostart autostart(autostartDir, "/tmp/synthetic dir/tail\"switch");
        QVERIFY(!autostart.isEnabled());
        QVERIFY(autostart.setEnabled(false)); // Disabling a missing entry is fine.
        QVERIFY(!QFile::exists(autostart.filePath()));

        QString error;
        QVERIFY2(autostart.setEnabled(true, &error), qPrintable(error));
        QVERIFY(autostart.isEnabled());
        QFile file(autostart.filePath());
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString contents = QString::fromUtf8(file.readAll());
        QVERIFY(contents.startsWith("[Desktop Entry]\n"));
        QVERIFY(contents.contains("Type=Application\n"));
        QVERIFY(contents.contains("Exec=\"/tmp/synthetic dir/tail\\\"switch\"\n"));
        QVERIFY(!contents.contains("Hidden=true"));
        // Re-enabling is idempotent and the file stays a single entry.
        QVERIFY(autostart.setEnabled(true, &error));
        QCOMPARE(QDir(autostartDir).entryList(QDir::Files).size(), 1);

        QVERIFY(autostart.setEnabled(false, &error));
        QVERIFY(!autostart.isEnabled());
        QVERIFY(!QFile::exists(autostart.filePath()));

        // An entry the user hid through KDE's autostart settings counts as off.
        QVERIFY(QDir().mkpath(autostartDir));
        QFile hidden(autostart.filePath());
        QVERIFY(hidden.open(QIODevice::WriteOnly));
        hidden.write("[Desktop Entry]\nType=Application\nHidden=true\n");
        hidden.close();
        QVERIFY(!autostart.isEnabled());

        Autostart relative(autostartDir, "tailswitch");
        QVERIFY(!relative.setEnabled(true, &error));
        QVERIFY(error.contains("absolute"));
        QCOMPARE(Autostart::quoteExec("/plain/path"), QString("\"/plain/path\""));
        QCOMPARE(Autostart::quoteExec("/a$b`c\\d"), QString("\"/a\\$b\\`c\\\\d\""));
    }

    void menuTogglesAutostart()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Autostart autostart(dir.path() + "/autostart", "/synthetic/tailswitch");
        KdeClipboard clipboard(nullptr, QDBusConnection::sessionBus(), "org.tailswitch.MissingClipboard");
        TailscaleClient client; // Never started.
        KStatusNotifierItem tray(QStringLiteral("tailswitch-autostart-test"));
        ReadOnlyTray controller(tray, client, clipboard, &autostart);
        auto *action = tray.contextMenu()->findChild<QAction *>("autostartAction");
        QVERIFY(action);
        QVERIFY(action->isCheckable());
        QVERIFY(!action->isChecked()); // Disabled by default.
        action->trigger();
        QVERIFY(action->isChecked());
        QVERIFY(autostart.isEnabled());
        action->trigger();
        QVERIFY(!action->isChecked());
        QVERIFY(!autostart.isEnabled());

        // Without an autostart manager the menu simply omits the item.
        KStatusNotifierItem plainTray(QStringLiteral("tailswitch-no-autostart-test"));
        ReadOnlyTray plain(plainTray, client, clipboard);
        QVERIFY(!plainTray.contextMenu()->findChild<QAction *>("autostartAction"));
    }

    void cleanupTestCase()
    {
        auto bus = QDBusConnection::sessionBus();
        bus.unregisterObject("/StatusNotifierWatcher");
        QVERIFY(bus.unregisterService("org.kde.StatusNotifierWatcher"));
        bus.unregisterObject("/org/freedesktop/Notifications");
        QVERIFY(bus.unregisterService("org.freedesktop.Notifications"));
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

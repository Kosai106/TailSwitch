#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCommandLineParser>
#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QSystemTrayIcon>
#include <QTextStream>
#include <QTimer>

#include <cstdio>

namespace {

QIcon prototypeIcon()
{
    // Neutral prototype artwork; no external assets or Tailscale branding.
    QIcon icon;
    for (const int size : {22, 32, 48, 64, 128}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.scale(size / 32.0, size / 32.0);
        painter.setPen(QPen(QColor("#60a5fa"), 2.5));
        painter.drawLine(QPointF(8, 9), QPointF(24, 16));
        painter.drawLine(QPointF(8, 23), QPointF(24, 16));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#60a5fa"));
        for (const auto point : {QPointF(8, 9), QPointF(8, 23), QPointF(24, 16)}) {
            painter.drawEllipse(point, 4, 4);
        }
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("TailSwitch");
    QApplication::setApplicationVersion("0.1.0-prototype");
    QApplication::setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Unofficial Tailscale tray compatibility prototype. No network controls yet.");
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption smokeTest(
        "smoke-test", "Report Qt/tray capabilities, briefly show the icon, then exit. "
                      "Does not access Tailscale or change the clipboard.");
    parser.addOption(smokeTest);
    parser.process(app);

    const bool trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    // Use stdout explicitly: the host's Qt logging can route qInfo to journald.
    QTextStream diagnostics(stdout);
    diagnostics << "Qt build: " << QT_VERSION_STR << " runtime: " << qVersion() << Qt::endl;
    diagnostics << "Platform: " << QGuiApplication::platformName() << Qt::endl;
    diagnostics << "System tray available: " << (trayAvailable ? "yes" : "no") << Qt::endl;
    diagnostics << "Tray reports notification support: "
                << (QSystemTrayIcon::supportsMessages() ? "yes" : "no") << Qt::endl;

    if (!trayAvailable) {
        QTextStream(stderr) << "No system tray is available. Run in KDE Plasma Desktop Mode."
                            << Qt::endl;
        return 2;
    }

    QMenu menu;
    menu.addAction("TailSwitch · Compatibility prototype")->setEnabled(false);
    menu.addAction("No Tailscale connection or device data loaded")->setEnabled(false);
    menu.addSeparator();

    auto *devices = menu.addMenu("Sample devices (synthetic)");
    auto *copySample = devices->addAction("Copy sample IPv4 · 100.64.0.1");
    auto *copyFeedback = menu.addAction("Clipboard test: not run");
    copyFeedback->setEnabled(false);
    QObject::connect(copySample, &QAction::triggered, &app, [copyFeedback] {
        // Only an explicit user action writes to the clipboard. Paste into a
        // separate app to verify transfer/ownership under Wayland.
        QApplication::clipboard()->setText("100.64.0.1", QClipboard::Clipboard);
        copyFeedback->setText("Sample IP sent to clipboard — paste to verify");
    });
    menu.addSeparator();
    auto *about = menu.addAction("About this prototype…");
    QObject::connect(about, &QAction::triggered, &app, [] {
        QMessageBox::about(nullptr, "About TailSwitch",
            "TailSwitch compatibility prototype\n\n"
            "An unofficial client, not affiliated with or endorsed by Tailscale.\n"
            "This prototype only tests the system tray and user-triggered clipboard copying.\n"
            "It does not read or change Tailscale settings.");
    });
    QObject::connect(menu.addAction("Quit"), &QAction::triggered,
                     &app, &QApplication::quit);

    QSystemTrayIcon tray(prototypeIcon());
    tray.setToolTip("TailSwitch — compatibility prototype (no network controls)");
    tray.setContextMenu(&menu);
    tray.show();

    if (parser.isSet(smokeTest)) {
        QTimer::singleShot(1500, &app, [&app, &tray, &diagnostics] {
            // Qt availability is a smoke check, not proof that the compositor
            // rendered the icon/menu correctly. That needs visual testing.
            const bool available = QSystemTrayIcon::isSystemTrayAvailable();
            diagnostics << "Tray availability after event processing: "
                        << (available ? "yes" : "no") << Qt::endl;
            tray.hide();
            app.exit(available ? 0 : 2);
        });
    }

    return app.exec();
}

#include "kdeclipboard.h"
#include "readonlytray.h"
#include "tailscaleclient.h"

#include <KStatusNotifierItem>
#include <QApplication>
#include <QCommandLineParser>
#include <QSystemTrayIcon>
#include <QTextStream>
#include <QTimer>

#include <cstdio>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("TailSwitch");
    QApplication::setApplicationVersion("0.1.0-readonly");
    QApplication::setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Unofficial, read-only Tailscale tray client. No network controls yet.");
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption smokeTest(
        "smoke-test", "Briefly show the native tray, report capabilities, then exit. "
                      "Does not access Tailscale or change the clipboard.");
    const QCommandLineOption checkStatus(
        "check-status", "Read status once and print only state/counts, without a tray. "
                        "Never prints device names, addresses, health text, or authentication URLs.");
    const QCommandLineOption executablePath(
        "tailscale-path", "Use an explicit absolute path to the Tailscale CLI.", "path");
    parser.addOptions({smokeTest, checkStatus, executablePath});
    parser.process(app);
    if (!parser.positionalArguments().isEmpty()
        || (parser.isSet(smokeTest) && parser.isSet(checkStatus))) {
        QTextStream(stderr) << "Use either --smoke-test or --check-status, without positional arguments."
                            << Qt::endl;
        return 1;
    }

    TailscaleClientOptions options;
    options.executable = parser.value(executablePath);
    TailscaleClient client(options);
    // Explicit stdout avoids host Qt logging routing diagnostics to journald.
    QTextStream diagnostics(stdout);
    if (parser.isSet(checkStatus)) {
        QObject::connect(&client, &TailscaleClient::statusReceived, &app,
                         [&app, &diagnostics](const TailscaleStatus &status) {
            diagnostics << "State: " << connectionLabel(status.state) << Qt::endl;
            diagnostics << "Peer count: " << status.peers.size() << Qt::endl;
            diagnostics << "Local IPv4 available: " << (!status.self.ipv4.isEmpty() ? "yes" : "no") << Qt::endl;
            diagnostics << "Health message count: " << status.health.size() << Qt::endl;
            app.exit(0);
        });
        QObject::connect(&client, &TailscaleClient::failed, &app,
                         [&app](const StatusFailure &failure) {
            QTextStream(stderr) << failure.message << Qt::endl;
            app.exit(1);
        });
        QTimer::singleShot(0, &client, &TailscaleClient::refresh);
        return app.exec();
    }

    const bool trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    diagnostics << "Qt build: " << QT_VERSION_STR << " runtime: " << qVersion() << Qt::endl;
    diagnostics << "Platform: " << QGuiApplication::platformName() << Qt::endl;
    diagnostics << "System tray available: " << (trayAvailable ? "yes" : "no") << Qt::endl;
    if (!trayAvailable) {
        QTextStream(stderr) << "No system tray is available. Run in KDE Plasma Desktop Mode." << Qt::endl;
        return 2;
    }

    KdeClipboard clipboard;
    KStatusNotifierItem tray(QStringLiteral("TailSwitch"));
    ReadOnlyTray controller(tray, client, clipboard);
    diagnostics << "Native menu-only tray: " << (tray.isMenu() ? "yes" : "no") << Qt::endl;
    if (parser.isSet(smokeTest)) {
        QTimer::singleShot(1500, &app, [&app, &tray, &diagnostics] {
            const bool available = QSystemTrayIcon::isSystemTrayAvailable();
            diagnostics << "Tray availability after event processing: "
                        << (available ? "yes" : "no") << Qt::endl;
            tray.setStatus(KStatusNotifierItem::Passive);
            app.exit(available && tray.isMenu() ? 0 : 2);
        });
    } else {
        QTimer::singleShot(0, &client, &TailscaleClient::start);
    }
    return app.exec();
}

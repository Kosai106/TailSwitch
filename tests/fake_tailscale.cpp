#include <QCoreApplication>
#include <QFile>
#include <QThread>

#include <csignal>
#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const auto args = app.arguments().mid(1);
    const auto logPath = qEnvironmentVariable("TAILSWITCH_FAKE_LOG");
    if (!logPath.isEmpty()) {
        QFile log(logPath);
        if (!log.open(QIODevice::WriteOnly | QIODevice::Append)) return 96;
        log.write(args.join('|').toUtf8() + '\n');
    }
    // Tests fail if the client ever invokes a mutation or extra arguments.
    if (args != QStringList{"status", "--json"}) return 97;
    QString scenario = qEnvironmentVariable("TAILSWITCH_FAKE_SCENARIO", "success");
    const auto controlPath = qEnvironmentVariable("TAILSWITCH_FAKE_CONTROL");
    if (!controlPath.isEmpty()) {
        QFile control(controlPath);
        if (!control.open(QIODevice::ReadOnly)) return 96;
        scenario = QString::fromUtf8(control.readAll()).trimmed();
    }
    if (scenario == "hang") {
        QThread::msleep(5000);
        return 98;
    }
    if (scenario == "crash") {
        std::raise(SIGKILL); // No core dump or descendant process.
        return 98;
    }
    if (scenario == "flood") {
        const QByteArray output(128 * 1024, 'x');
        std::fwrite(output.data(), 1, size_t(output.size()), stderr);
        std::fflush(stderr);
        QThread::msleep(5000);
        return 98;
    }
    if (scenario == "permission" || scenario == "daemon" || scenario == "login-text" || scenario == "failure") {
        const char *message = scenario == "permission" ? "access denied: private-fixture-detail\n"
            : scenario == "daemon" ? "failed to connect to local tailscaled: private-fixture-detail\n"
            : scenario == "login-text" ? "not logged in: private-fixture-detail\n"
            : "unexpected error: private-fixture-detail\n";
        std::fputs(message, stderr);
        return 1;
    }
    if (scenario == "bad-json") {
        std::fputs("not JSON: private-fixture-detail", stdout);
        return 0;
    }
    if (scenario == "bad-schema") {
        std::fputs("{\"BackendState\":\"Running\",\"Peer\":[]}", stdout);
        return 0;
    }
    const auto fileName = scenario == "stopped" ? "status-stopped.json"
        : scenario == "login" ? "status-login.json" : "status-running.json";
    QFile fixture(QStringLiteral(TAILSWITCH_FIXTURE_DIR) + '/' + fileName);
    if (!fixture.open(QIODevice::ReadOnly)) return 96;
    const auto json = fixture.readAll();
    if (scenario == "fragmented") {
        const auto half = json.size() / 2;
        std::fwrite(json.data(), 1, size_t(half), stdout);
        std::fflush(stdout);
        QThread::msleep(50);
        std::fwrite(json.data() + half, 1, size_t(json.size() - half), stdout);
    } else {
        std::fwrite(json.data(), 1, size_t(json.size()), stdout);
    }
    return scenario == "stopped" || scenario == "login" || scenario == "running-exit1" ? 1 : 0;
}

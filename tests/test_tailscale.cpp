#include "tailscaleclient.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace {
QByteArray fixture(const QString &name)
{
    QFile file(QStringLiteral(TAILSWITCH_FIXTURE_DIR) + '/' + name);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

TailscaleClientOptions options(const QString &scenario)
{
    TailscaleClientOptions result;
    result.executable = QStringLiteral(FAKE_TAILSCALE_PATH);
    result.environment.insert("TAILSWITCH_FAKE_SCENARIO", scenario);
    result.environment.remove("TAILSWITCH_FAKE_CONTROL");
    result.environment.remove("TAILSWITCH_FAKE_LOG");
    result.timeoutMs = 1000;
    return result;
}

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(contents) == contents.size();
}
} // namespace

class TailscaleTest final : public QObject
{
    Q_OBJECT
private slots:
    void parsesAndSortsDevices()
    {
        const auto parsed = parseTailscaleStatus(fixture("status-running.json"));
        QVERIFY2(parsed.status.has_value(), qPrintable(parsed.error));
        const auto &status = *parsed.status;
        QCOMPARE(status.state, ConnectionState::Connected);
        QCOMPARE(status.self.ipv4, QString("100.64.0.1"));
        QCOMPARE(status.peers.size(), 5);
        QCOMPARE(status.peers[0].name, QString("alpha & dev"));
        QCOMPARE(status.peers[1].name, QString("Zulu"));
        QCOMPARE(status.peers[1].ipv4, QString("100.64.0.2"));
        QCOMPARE(status.peers[2].name, QString("aardvark"));
        QVERIFY(status.peers[2].online.has_value() && !*status.peers[2].online);
        QCOMPARE(status.peers[2].ipv4, QString("100.64.0.4"));
        QCOMPARE(status.peers[3].name, QString("ipv6-only"));
        QVERIFY(status.peers[3].ipv4.isEmpty());
        QVERIFY(!status.peers[4].online.has_value());
        QCOMPARE(status.health, QStringList{"Synthetic DNS warning"});
    }

    void minimalStates_data()
    {
        QTest::addColumn<QString>("backend");
        QTest::addColumn<int>("expected");
        QTest::newRow("running") << "Running" << int(ConnectionState::Connected);
        QTest::newRow("stopped") << "Stopped" << int(ConnectionState::Disconnected);
        QTest::newRow("login") << "NeedsLogin" << int(ConnectionState::NeedsLogin);
        QTest::newRow("approval") << "NeedsMachineAuth" << int(ConnectionState::NeedsApproval);
        QTest::newRow("starting") << "Starting" << int(ConnectionState::Starting);
        QTest::newRow("initializing") << "NoState" << int(ConnectionState::Initializing);
        QTest::newRow("other-user") << "InUseOtherUser" << int(ConnectionState::InUseOtherUser);
    }

    void minimalStates()
    {
        QFETCH(QString, backend);
        QFETCH(int, expected);
        const auto parsed = parseTailscaleStatus(QJsonDocument(QJsonObject{{"BackendState", backend}}).toJson());
        QVERIFY(parsed.status.has_value());
        QCOMPARE(int(parsed.status->state), expected);
        QVERIFY(parsed.status->peers.isEmpty());
        QVERIFY(parsed.status->self.ipv4.isEmpty());
    }

    void invalidStructures_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("bad-json") << QByteArray("private-fixture-detail");
        QTest::newRow("array") << QByteArray("[]");
        QTest::newRow("missing-state") << QByteArray("{}");
        QTest::newRow("wrong-state-type") << QByteArray(R"({"BackendState":true})");
        QTest::newRow("future-state") << QByteArray(R"({"BackendState":"FutureState"})");
        QTest::newRow("peers-array") << QByteArray(R"({"BackendState":"Running","Peer":[]})");
        QTest::newRow("bad-peer") << QByteArray(R"({"BackendState":"Running","Peer":{"synthetic":false}})");
        QTest::newRow("bad-self") << QByteArray(R"({"BackendState":"Running","Self":[]})");
        QTest::newRow("bad-online") << QByteArray(R"({"BackendState":"Running","Self":{"Online":"yes"}})");
        QTest::newRow("bad-address") << QByteArray(R"({"BackendState":"Running","TailscaleIPs":["invalid"]})");
        QTest::newRow("wrong-address-type") << QByteArray(R"({"BackendState":"Running","TailscaleIPs":[42]})");
        QTest::newRow("bad-health") << QByteArray(R"({"BackendState":"Running","Health":[42]})");
        QTest::newRow("bad-name") << QByteArray(R"({"BackendState":"Running","Self":{"HostName":42}})");
    }

    void invalidStructures()
    {
        QFETCH(QByteArray, json);
        const auto parsed = parseTailscaleStatus(json);
        QVERIFY(!parsed.status.has_value());
        QVERIFY(!parsed.error.isEmpty());
        QVERIFY(!parsed.error.contains("private-fixture-detail"));
    }

    void fallbackAddressesAndMenuText()
    {
        const auto parsed = parseTailscaleStatus(fixture("status-stopped.json"));
        QVERIFY(parsed.status.has_value());
        QCOMPARE(parsed.status->self.ipv4, QString("100.64.0.1"));
        QCOMPARE(menuText("A&B\nC\tD"), QString("A&&B C D"));
        QCOMPARE(menuText(QString(1000, 'x')).size(), 160);
        const auto login = parseTailscaleStatus(fixture("status-login.json"));
        QVERIFY(login.status.has_value());
        QVERIFY(!connectionGuidance(login.status->state).contains("synthetic-secret"));
    }

    void successfulReads_data()
    {
        QTest::addColumn<QString>("scenario");
        QTest::addColumn<int>("expected");
        QTest::newRow("running") << "success" << int(ConnectionState::Connected);
        QTest::newRow("fragmented") << "fragmented" << int(ConnectionState::Connected);
        QTest::newRow("stopped-exit1") << "stopped" << int(ConnectionState::Disconnected);
        QTest::newRow("login-exit1") << "login" << int(ConnectionState::NeedsLogin);
    }

    void successfulReads()
    {
        QFETCH(QString, scenario);
        QFETCH(int, expected);
        TailscaleClient client(options(scenario));
        QSignalSpy success(&client, &TailscaleClient::statusReceived);
        QSignalSpy failure(&client, &TailscaleClient::failed);
        client.refresh();
        QVERIFY(client.isBusy());
        QTRY_COMPARE(success.count(), 1);
        QCOMPARE(failure.count(), 0);
        QVERIFY(!client.isBusy());
        QCOMPARE(int(qvariant_cast<TailscaleStatus>(success[0][0]).state), expected);
    }

    void failedReads_data()
    {
        QTest::addColumn<QString>("scenario");
        QTest::addColumn<int>("expected");
        QTest::newRow("denied") << "permission" << int(StatusError::PermissionDenied);
        QTest::newRow("daemon") << "daemon" << int(StatusError::DaemonUnavailable);
        QTest::newRow("login-text") << "login-text" << int(StatusError::LoginRequired);
        QTest::newRow("failure") << "failure" << int(StatusError::CommandFailed);
        QTest::newRow("running-exit1") << "running-exit1" << int(StatusError::CommandFailed);
        QTest::newRow("bad-json") << "bad-json" << int(StatusError::InvalidOutput);
        QTest::newRow("bad-schema") << "bad-schema" << int(StatusError::InvalidOutput);
        QTest::newRow("crash") << "crash" << int(StatusError::CommandFailed);
        QTest::newRow("timeout") << "hang" << int(StatusError::TimedOut);
        QTest::newRow("output-limit") << "flood" << int(StatusError::OutputTooLarge);
    }

    void failedReads()
    {
        QFETCH(QString, scenario);
        QFETCH(int, expected);
        auto config = options(scenario);
        config.timeoutMs = scenario == "hang" ? 150 : 1000;
        config.maxOutputBytes = 8192;
        TailscaleClient client(config);
        QSignalSpy success(&client, &TailscaleClient::statusReceived);
        QSignalSpy failure(&client, &TailscaleClient::failed);
        client.refresh();
        QTRY_COMPARE(failure.count(), 1);
        QCOMPARE(success.count(), 0);
        QVERIFY(!client.isBusy());
        const auto error = qvariant_cast<StatusFailure>(failure[0][0]);
        QCOMPARE(int(error.code), expected);
        QVERIFY(!error.message.contains("private-fixture-detail"));
        QTest::qWait(30);
        QCOMPARE(failure.count(), 1);
    }

    void missingAndBrokenExecutables()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto config = options("success");
        config.executable = dir.filePath("missing tailscale");
        TailscaleClient missing(config);
        QSignalSpy missingFailure(&missing, &TailscaleClient::failed);
        missing.refresh();
        QCOMPARE(missingFailure.count(), 1);
        QCOMPARE(qvariant_cast<StatusFailure>(missingFailure[0][0]).code, StatusError::CliMissing);
        QVERIFY(TailscaleClient::discoverExecutable("tailscale --up").isEmpty());

        config.executable = dir.filePath("broken executable");
        QVERIFY(writeFile(config.executable, "#!/nonexistent/tailswitch-test-interpreter\n"));
        QVERIFY(QFile::setPermissions(config.executable, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        TailscaleClient broken(config);
        QSignalSpy brokenFailure(&broken, &TailscaleClient::failed);
        broken.refresh();
        QTRY_COMPARE(brokenFailure.count(), 1);
        QCOMPARE(qvariant_cast<StatusFailure>(brokenFailure[0][0]).code, StatusError::FailedToStart);
    }

    void coalescesRequestsAndRecovers()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto control = dir.filePath("scenario");
        const auto log = dir.filePath("commands");
        QVERIFY(writeFile(control, "hang"));
        auto config = options("unused");
        // A path containing spaces must work without any shell quoting.
        const auto linkedExecutable = dir.filePath("fake tailscale");
        QVERIFY(QFile::link(QStringLiteral(FAKE_TAILSCALE_PATH), linkedExecutable));
        config.executable = linkedExecutable;
        config.environment.insert("TAILSWITCH_FAKE_CONTROL", control);
        config.environment.insert("TAILSWITCH_FAKE_LOG", log);
        config.timeoutMs = 200;
        TailscaleClient client(config);
        QSignalSpy success(&client, &TailscaleClient::statusReceived);
        QSignalSpy failure(&client, &TailscaleClient::failed);
        client.refresh();
        client.refresh();
        client.refresh();
        QTRY_COMPARE(failure.count(), 1);
        QVERIFY(writeFile(control, "success"));
        client.refresh();
        QTRY_COMPARE(success.count(), 1);
        QFile commands(log);
        QVERIFY(commands.open(QIODevice::ReadOnly));
        QCOMPARE(commands.readAll(), QByteArray("status|--json\nstatus|--json\n"));
        QCOMPARE(failure.count(), 1);
    }

    void periodicRefresh()
    {
        auto config = options("success");
        config.pollIntervalMs = 100;
        TailscaleClient client(config);
        QSignalSpy success(&client, &TailscaleClient::statusReceived);
        client.start();
        QTRY_VERIFY(success.count() >= 2);
    }
};

QTEST_GUILESS_MAIN(TailscaleTest)
#include "test_tailscale.moc"

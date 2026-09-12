#include "tailscaleclient.h"

#include <QFileInfo>
#include <QStandardPaths>

namespace {

StatusFailure classifyFailure(const QByteArray &output)
{
    // Best-effort classification only; never show or log raw CLI output.
    // A successful JSON BackendState is preferred over these text hints.
    const auto text = output.toLower();
    if (text.contains("permission denied") || text.contains("access denied")
        || text.contains("requires root") || text.contains("must be root")) {
        return {StatusError::PermissionDenied,
            "Your user cannot read Tailscale status. Check Tailscale's permissions/operator configuration with an administrator. Do not run TailSwitch as root."};
    }
    if (text.contains("failed to connect to local tailscaled") || text.contains("tailscaled is not running")
        || text.contains("connection refused") || text.contains("failed to connect to tailscaled")) {
        return {StatusError::DaemonUnavailable,
            "Cannot reach the local tailscaled service. Check that Tailscale is installed and its service is running, then refresh."};
    }
    if (text.contains("logged out") || text.contains("not logged in") || text.contains("needs login")) {
        return {StatusError::LoginRequired,
            "Tailscale needs authentication. Complete or renew your login using the CLI, then refresh."};
    }
    return {StatusError::CommandFailed,
        "The Tailscale status command failed. Check the CLI/service manually and refresh. Raw command output is intentionally not displayed."};
}

bool executableFile(const QString &path)
{
    const QFileInfo info(path);
    return info.isAbsolute() && info.isFile() && info.isExecutable();
}

} // namespace

TailscaleClient::TailscaleClient(const TailscaleClientOptions &options, QObject *parent)
    : QObject(parent), m_options(options)
{
    m_timeout.setSingleShot(true);
    m_poll.setInterval(qMax(100, m_options.pollIntervalMs));
    connect(&m_poll, &QTimer::timeout, this, &TailscaleClient::refresh);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        abortRead({StatusError::TimedOut,
            "Tailscale status timed out. Check the local service and refresh; TailSwitch has not changed any network settings."});
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &TailscaleClient::readOutput);
    connect(&m_process, &QProcess::readyReadStandardError, this, &TailscaleClient::readOutput);
    connect(&m_process, &QProcess::finished, this, &TailscaleClient::onFinished);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            if (m_aborted) {
                finishFailure(*m_aborted);
            } else {
                finishFailure({StatusError::FailedToStart,
                    "The Tailscale CLI could not start. Check the executable path, permissions, and runtime dependencies."});
            }
        }
        // CrashExit is handled by finished(), including our own timeout kill.
    });
}

TailscaleClient::~TailscaleClient()
{
    m_poll.stop();
    m_timeout.stop();
    m_process.disconnect(this);
    if (m_process.state() != QProcess::NotRunning) {
        // This child performs only a read; no daemon service is stopped.
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

QString TailscaleClient::discoverExecutable(const QString &explicitPath)
{
    if (!explicitPath.isEmpty()) {
        return executableFile(explicitPath) ? explicitPath : QString{};
    }
    const QString fromPath = QStandardPaths::findExecutable("tailscale");
    if (executableFile(fromPath)) return fromPath;
    // SteamOS installation on this Deck is outside the usual desktop PATH.
    for (const auto &path : {QStringLiteral("/opt/tailscale/tailscale"),
                             QStringLiteral("/usr/local/bin/tailscale"),
                             QStringLiteral("/usr/bin/tailscale")}) {
        if (executableFile(path)) return path;
    }
    return {};
}

void TailscaleClient::start()
{
    m_poll.start();
    refresh();
}

void TailscaleClient::refresh()
{
    // Coalesce manual and timer refreshes: never spawn overlapping commands.
    if (m_busy) return;
    m_busy = true;
    emit busyChanged(true);
    m_stdout.clear();
    m_stderr.clear();
    m_outputBytes = 0;
    m_aborted.reset();
    const auto executable = discoverExecutable(m_options.executable);
    if (executable.isEmpty()) {
        finishFailure({StatusError::CliMissing,
            "Tailscale CLI not found or not executable. Install Tailscale, or launch TailSwitch with --tailscale-path /absolute/path/to/tailscale."});
        return;
    }
    auto environment = m_options.environment;
    environment.insert("LC_ALL", "C");
    m_process.setProcessEnvironment(environment);
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_process.setProgram(executable);
    m_process.setArguments({"status", "--json"});
    m_timeout.start(qMax(1, m_options.timeoutMs));
    m_process.start(QIODevice::ReadOnly);
}

void TailscaleClient::readOutput()
{
    const auto out = m_process.readAllStandardOutput();
    const auto err = m_process.readAllStandardError();
    if (!m_busy || m_aborted) return;
    m_outputBytes += out.size() + err.size();
    if (m_outputBytes > qMax(qsizetype(1), m_options.maxOutputBytes)) {
        m_stdout.clear();
        m_stderr.clear();
        abortRead({StatusError::OutputTooLarge,
            "Tailscale status exceeded the output safety limit. Check the CLI version and tailnet size before retrying."});
        return;
    }
    m_stdout.append(out);
    m_stderr.append(err);
}

void TailscaleClient::abortRead(const StatusFailure &failure)
{
    if (!m_busy || m_aborted) return;
    m_aborted = failure;
    m_timeout.stop();
    if (m_process.state() == QProcess::NotRunning) {
        finishFailure(failure);
    } else {
        m_process.kill();
        // Keep busy until finished: a subsequent request must not race the kill.
    }
}

void TailscaleClient::finishFailure(const StatusFailure &failure)
{
    if (!m_busy) return;
    m_timeout.stop();
    m_busy = false;
    m_stdout.clear();
    m_stderr.clear();
    emit failed(failure);
    emit busyChanged(false);
}

void TailscaleClient::onFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!m_busy) return;
    readOutput();
    if (!m_busy) return;
    m_timeout.stop();
    if (m_aborted) {
        finishFailure(*m_aborted);
        return;
    }
    if (exitStatus != QProcess::NormalExit) {
        finishFailure({StatusError::CommandFailed, "The Tailscale status process exited unexpectedly. Refresh or check the CLI installation."});
        return;
    }
    const auto parsed = parseTailscaleStatus(m_stdout);
    if (parsed.status && (exitCode == 0 || parsed.status->state != ConnectionState::Connected)) {
        // CLI releases can return nonzero while still describing a valid
        // stopped/logged-out backend. Do not turn that into a daemon error.
        m_stdout.clear();
        m_stderr.clear();
        m_busy = false;
        emit statusReceived(*parsed.status);
        emit busyChanged(false);
        return;
    }
    if (exitCode == 0) {
        finishFailure({StatusError::InvalidOutput, parsed.error});
    } else {
        // Do not classify names/health fields inside a JSON document as CLI
        // error messages. Text classification is only a fallback for failures.
        const auto trimmed = m_stdout.trimmed();
        const bool looksLikeJson = trimmed.startsWith('{') || trimmed.startsWith('[');
        finishFailure(classifyFailure(m_stderr + (looksLikeJson ? QByteArray{} : m_stdout)));
    }
}

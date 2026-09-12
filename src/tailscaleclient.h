#pragma once

#include "tailscalestatus.h"

#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>

enum class StatusError {
    CliMissing, FailedToStart, DaemonUnavailable, PermissionDenied,
    LoginRequired, TimedOut, OutputTooLarge, InvalidOutput, CommandFailed
};

struct StatusFailure {
    StatusError code = StatusError::CommandFailed;
    QString message;
};
Q_DECLARE_METATYPE(StatusFailure)

struct TailscaleClientOptions {
    QString executable; // Optional explicit absolute path; no shell command.
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    int timeoutMs = 5000;
    int pollIntervalMs = 10000;
    qsizetype maxOutputBytes = 8 * 1024 * 1024;
};

class TailscaleClient final : public QObject
{
    Q_OBJECT
public:
    explicit TailscaleClient(const TailscaleClientOptions &options = {}, QObject *parent = nullptr);
    ~TailscaleClient() override;
    bool isBusy() const { return m_busy; }
    static QString discoverExecutable(const QString &explicitPath = {});

public slots:
    void start();
    void refresh();

signals:
    void statusReceived(const TailscaleStatus &status);
    void failed(const StatusFailure &failure);
    void busyChanged(bool busy);

private:
    void readOutput();
    void abortRead(const StatusFailure &failure);
    void finishFailure(const StatusFailure &failure);
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);

    TailscaleClientOptions m_options;
    QProcess m_process;
    QTimer m_timeout;
    QTimer m_poll;
    QByteArray m_stdout;
    QByteArray m_stderr;
    qsizetype m_outputBytes = 0;
    bool m_busy = false;
    std::optional<StatusFailure> m_aborted;
};

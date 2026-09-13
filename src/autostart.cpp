#include "autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>

namespace {
constexpr auto kEntryName = "tailswitch.desktop";
}

Autostart::Autostart(QString directory, QString executable)
    : m_directory(std::move(directory)), m_executable(std::move(executable))
{
}

Autostart Autostart::forCurrentUser()
{
    const QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return Autostart(config + QStringLiteral("/autostart"),
                     QCoreApplication::applicationFilePath());
}

QString Autostart::filePath() const
{
    return m_directory + QLatin1Char('/') + QLatin1String(kEntryName);
}

bool Autostart::isEnabled() const
{
    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    // A Hidden entry is the XDG way of disabling autostart while keeping the
    // file; treat it as disabled rather than reporting a stale checkmark.
    const auto contents = QString::fromUtf8(file.readAll());
    for (const auto &line : contents.split(QLatin1Char('\n'))) {
        const auto trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1String("Hidden=")) && trimmed.mid(7).trimmed() == QLatin1String("true")) {
            return false;
        }
    }
    return true;
}

QString Autostart::quoteExec(const QString &executable)
{
    // Desktop Entry spec: quote the argument and backslash-escape the
    // characters that are reserved inside quotes.
    QString quoted;
    quoted.reserve(executable.size() + 2);
    quoted += QLatin1Char('"');
    for (const QChar character : executable) {
        if (character == QLatin1Char('"') || character == QLatin1Char('`')
            || character == QLatin1Char('$') || character == QLatin1Char('\\')) {
            quoted += QLatin1Char('\\');
        }
        quoted += character;
    }
    quoted += QLatin1Char('"');
    return quoted;
}

bool Autostart::setEnabled(bool enabled, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error) *error = message;
        return false;
    };
    if (!enabled) {
        QFile file(filePath());
        if (!file.exists()) return true;
        if (!file.remove()) {
            return fail(QStringLiteral("Could not remove %1: %2").arg(filePath(), file.errorString()));
        }
        return true;
    }
    if (m_executable.isEmpty() || !QDir::isAbsolutePath(m_executable)) {
        return fail(QStringLiteral("The TailSwitch executable path is not absolute; cannot write an autostart entry."));
    }
    if (!QDir().mkpath(m_directory)) {
        return fail(QStringLiteral("Could not create %1.").arg(m_directory));
    }
    const QString contents = QStringLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=TailSwitch\n"
        "Comment=Tailscale status in the system tray\n"
        "Exec=%1\n"
        "Icon=tailswitch\n"
        "Terminal=false\n"
        "StartupNotify=false\n"
        "Categories=Network;Qt;KDE;\n"
        "X-KDE-autostart-after=panel\n"
        "X-TailSwitch-Generated=true\n").arg(quoteExec(m_executable));
    QSaveFile file(filePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return fail(QStringLiteral("Could not write %1: %2").arg(filePath(), file.errorString()));
    }
    const auto bytes = contents.toUtf8();
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        return fail(QStringLiteral("Could not write %1: %2").arg(filePath(), file.errorString()));
    }
    return true;
}

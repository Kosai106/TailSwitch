#pragma once

#include <QString>

// Opt-in launch at Desktop Mode login, implemented as an XDG autostart entry
// in the user's configuration directory. Disabled by default; never touches
// Tailscale itself. Enabling autostart only starts this tray application.
class Autostart final
{
public:
    // directory: the XDG autostart directory to manage (for example
    // ~/.config/autostart). executable: absolute path launched at login.
    Autostart(QString directory, QString executable);

    // Uses the current user's configuration directory and this executable.
    static Autostart forCurrentUser();

    QString filePath() const;
    bool isEnabled() const;
    // Returns false and fills `error` when the entry cannot be written or removed.
    bool setEnabled(bool enabled, QString *error = nullptr);

    // Desktop-entry Exec value for `executable`, quoted per the XDG spec.
    static QString quoteExec(const QString &executable);

private:
    QString m_directory;
    QString m_executable;
};

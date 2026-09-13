# Changelog

All notable changes to TailSwitch are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/).

## [0.1.0] - 2026-09-13

Initial public release: a read-only Tailscale status tray for KDE Plasma on
Steam Deck. This version never changes Tailscale settings, starts a login, or
disconnects you.

### Added

- Native Plasma tray icon with the same menu on left-click and right-click.
- Connection state in the menu header and tooltip, distinguishing connected,
  disconnected, connecting, sign-in required, device approval required, in use
  by another user, and initializing.
- **This device** and **Devices** submenus that copy a device's Tailscale IPv4
  through KDE's clipboard. Online devices sort first, offline devices remain
  copyable, and devices without an IPv4 are shown disabled.
- **Status details** dialog with guidance for the current state, the CLI
  version, and Tailscale's own health messages shown locally as plain text.
- Automatic refresh every 10 seconds plus **Refresh now**, with a timeout,
  output limit, and one notification per distinct failure.
- **Start at login**, off by default, managed through a standard autostart
  entry in the user's configuration directory.
- Single-instance guard so launching TailSwitch again does not add a second
  tray icon.
- Home-directory installer and uninstaller; nothing outside `$HOME` is touched.
- `--check-status` and `--smoke-test` diagnostics that print no device names,
  addresses, or health text.

[0.1.0]: https://github.com/Kosai106/TailSwitch/releases/tag/v0.1.0

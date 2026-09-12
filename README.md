# TailSwitch

A lightweight, unofficial Tailscale system-tray app for KDE Plasma on Steam Deck, built with C++ and Qt 6 Widgets.

**Status:** working read-only preview for the Steam Deck's KDE Wayland session. Displays real Tailscale connection state, this device's IPv4, and sorted peers with click-to-copy. Refreshes every 10 seconds and reports errors/health warnings. Connection and exit-node controls are not implemented yet.

## Available now

- Native Plasma menus on both left-click and right-click.
- Embedded, tightly cropped Tailscale logo adapted to the current light/dark application palette.
- Real connection state, with login/approval/initializing/disconnected states distinguished. Routine health messages stay in the header/details without pulsing the tray icon.
- This device and peers: click to copy IPv4 through KDE Clipboard. Offline peers remain copyable; IPv6-only entries are disabled.
- Automatic refresh, **Refresh now**, and **Status details** with locally displayed Tailscale health messages.
- Bounded asynchronous CLI reads, failure recovery, and no repeated notifications for an unchanged error.

## Planned v1

- Connect and disconnect without changing existing Tailscale preferences.
- List devices, including offline peers, and click to copy their Tailscale IPv4.
- Select an exit node and control local-network access while using one.
- Optional launch at Desktop Mode login.
- Run unprivileged against an existing, installed and authenticated Tailscale service.
- Install in the user's home directory without modifying SteamOS's read-only system.

Quitting the app will not disconnect Tailscale. An unavailable exit node will never be silently disabled or replaced.

## Project documents

- [Approved plan](docs/PLAN.md)
- [Compatibility findings and remaining checks](docs/COMPATIBILITY.md)
- [Build and manual smoke-test instructions](docs/DEVELOPMENT.md)
- [Follow-ups, including old-container cleanup](docs/TODO.md)

## Development

Stack: C++17 / Qt 6 Widgets / KDE Frameworks StatusNotifierItem / CMake. Build in the `tailswitch-kde-dev` Ubuntu 26.04 Distrobox container (Qt >= 6.8, KF6StatusNotifierItem >= 6.14):

```sh
cd /home/deck/Developer/TailSwitch
distrobox enter tailswitch-kde-dev -- cmake -S "$PWD" -B "$PWD/build/kde-dev" -G Ninja -DCMAKE_BUILD_TYPE=Debug
distrobox enter tailswitch-kde-dev -- cmake --build "$PWD/build/kde-dev"
distrobox enter tailswitch-kde-dev -- ctest --test-dir "$PWD/build/kde-dev" --output-on-failure
```

The replacement passed user validation and the old `tailswitch-dev` container has been removed. Its ignored `build/dev` artifacts remain; use only `build/kde-dev` for current builds and launches.

Run on the host:

```sh
QT_QPA_PLATFORM=wayland ./build/kde-dev/tailswitch --smoke-test
# Omit --smoke-test to read live Tailscale status and use the tray menu.
```

This is a dynamically linked development build, not a portable release. The native tray item exports `ItemIsMenu=true` so Plasma can present the menu for both left-click and right-click, without an app-owned Wayland popup. The user confirmed the native tray replacement and real-device/copy workflow work. Copying uses Plasma's Clipboard manager (Klipper) via D-Bus.

For a one-shot read that prints only state/counts (no tray or clipboard access):

```sh
QT_QPA_PLATFORM=offscreen ./build/kde-dev/tailswitch --check-status
```

CLI discovery uses PATH plus common paths, including `/opt/tailscale/tailscale`. Override it with `--tailscale-path /absolute/path/to/tailscale` if needed. The app only invokes `tailscale status --json`; it never starts login or changes preferences.

Automated coverage includes synthetic status fixtures, a fake CLI, timeout/crash/output-limit/error/recovery tests, periodic refresh, native D-Bus menu export, live menu reconciliation and clipboard behavior through fake services, sanitized status-check output, and ELF copy-relocation checks. Run tests in Distrobox with its matching Qt Test runtime.

Do not commit real tailnet status, device identifiers, login URLs, credentials, or raw preference dumps. Test fixtures must use synthetic data.

## Name and affiliation

TailSwitch is a provisional name pending availability checks; TailTray is the preferred fallback, also subject to checking. This project is not affiliated with or endorsed by Tailscale.

## License

Our code is licensed under [MIT](LICENSE). Qt, KDE Frameworks, and any other bundled third-party components retain their own licenses; release packaging must include the required notices and satisfy their redistribution obligations. The Tailscale logo is not covered by TailSwitch's MIT license; see [the asset notice](assets/README.md).

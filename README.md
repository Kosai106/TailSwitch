# TailSwitch

A planned lightweight, unofficial Tailscale system-tray app for KDE Plasma on Steam Deck, built with C++ and Qt 6 Widgets.

**Status:** compatibility prototype builds and runs in the Steam Deck's KDE Wayland session. It provides a tray menu and manual copying of a synthetic IP; real Tailscale integration is not implemented yet.

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
# Or omit --smoke-test to explore the tray menu and manually test copying.
```

This is a dynamically linked development build, not a portable release. The native tray item exports `ItemIsMenu=true` so Plasma can present the menu for both left-click and right-click, without an app-owned Wayland popup. The user confirmed the native tray replacement works. Copying uses Plasma's Clipboard manager (Klipper) via D-Bus; the user confirmed this clipboard approach works.

Automated coverage checks CLI help, native D-Bus tray-menu export, menu ownership, clipboard success/failure through a fake service, and absence of ELF copy relocations. Run tests in Distrobox with its matching Qt Test runtime. Fake-CLI tests are planned with the actual Tailscale adapter.

Do not commit real tailnet status, device identifiers, login URLs, credentials, or raw preference dumps. Test fixtures must use synthetic data.

## Name and affiliation

TailSwitch is a provisional name pending availability checks; TailTray is the preferred fallback, also subject to checking. This project is not affiliated with or endorsed by Tailscale.

## License

Our code is licensed under [MIT](LICENSE). Qt, KDE Frameworks, and any other bundled third-party components retain their own licenses; release packaging must include the required notices and satisfy their redistribution obligations.

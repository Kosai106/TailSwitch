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

## Development

Stack: C++17 / Qt 6 Widgets / CMake. Build in the `tailswitch-dev` Distrobox container:

```sh
cd /home/deck/Developer/TailSwitch
distrobox enter tailswitch-dev -- cmake -S "$PWD" -B "$PWD/build/dev" -G Ninja -DCMAKE_BUILD_TYPE=Debug
distrobox enter tailswitch-dev -- cmake --build "$PWD/build/dev"
distrobox enter tailswitch-dev -- ctest --test-dir "$PWD/build/dev" --output-on-failure
```

Run on the host:

```sh
QT_QPA_PLATFORM=wayland ./build/dev/tailswitch --smoke-test
# Or omit --smoke-test to explore the tray menu and manually test copying.
```

This is a dynamically linked development build, not a portable release. Left-click opens the tray menu; right-click remains supported. Copying uses Plasma's Clipboard manager (Klipper) via D-Bus to avoid Wayland input-focus restrictions.

Automated coverage checks CLI help, tray activation, and clipboard success/failure using a fake service on an isolated session bus. Run these tests in Distrobox with its matching Qt Test runtime. Fake-CLI behavioral tests are planned with the actual Tailscale adapter.

Do not commit real tailnet status, device identifiers, login URLs, credentials, or raw preference dumps. Test fixtures must use synthetic data.

## Name and affiliation

TailSwitch is a provisional name pending availability checks; TailTray is the preferred fallback, also subject to checking. This project is not affiliated with or endorsed by Tailscale.

## License

Our code is licensed under [MIT](LICENSE). Qt and any bundled third-party components retain their own licenses; release packaging must include the required notices and satisfy their redistribution obligations.

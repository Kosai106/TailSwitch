# TailSwitch

A small, unofficial Tailscale tray for KDE Plasma on the Steam Deck. It sits in the Desktop Mode system tray, shows whether Tailscale is connected, and lets you copy any device's Tailscale IPv4 address with one click.

TailSwitch is not affiliated with or endorsed by Tailscale Inc.

## What it does

- Shows the connection state in the tray menu and tooltip: connected, disconnected, connecting, sign-in required, device approval required, and more.
- Lists this device and every peer in your tailnet, online devices first. Click a device to copy its Tailscale IPv4. Offline devices stay copyable; devices without an IPv4 are shown disabled.
- Opens the same menu on left-click and right-click, using Plasma's native tray menu.
- Refreshes every 10 seconds, or on demand with **Refresh now**.
- **Status details** shows guidance for the current state, the Tailscale CLI version, and Tailscale's own health messages.
- **Start at login** (off by default) launches TailSwitch with each Desktop Mode login.

## What it does not do

TailSwitch 0.1 is a status view. It only ever runs `tailscale status --json`.

- It does not connect, disconnect, choose exit nodes, or change any Tailscale setting. Use the `tailscale` CLI or Tailscale's own tools for that.
- It does not sign you in. Tailscale must already be installed and logged in.
- It does not run as root, store credentials, log device names or addresses, or send anything anywhere.
- Quitting or uninstalling it leaves Tailscale exactly as it was.

Connection and exit-node controls are planned; see [the roadmap](docs/PLAN.md).

## Requirements

- A Steam Deck in Desktop Mode (KDE Plasma 6, Wayland). Other KDE Plasma 6 desktops on x86_64 Linux may work but are untested.
- SteamOS 3.9 or newer. The prebuilt binary links against the Qt 6.10 and KDE Frameworks 6.14 libraries that ship with SteamOS, so it will not load on older releases. Only SteamOS 3.10 has been tested.
- Tailscale installed and logged in, with the CLI at `/opt/tailscale/tailscale`, `/usr/bin/tailscale`, `/usr/local/bin/tailscale`, or on your PATH. Tested with Tailscale 1.102.3.

## Install

Everything goes into your home directory; SteamOS's read-only system is not modified and no password is needed.

1. Download `tailswitch-<version>-linux-x86_64.tar.gz` from the [latest release](https://github.com/Kosai106/TailSwitch/releases/latest).
2. In Konsole:

   ```sh
   tar xzf tailswitch-*-linux-x86_64.tar.gz
   cd tailswitch-*-linux-x86_64
   ./install.sh
   ```

3. Launch **TailSwitch** from the application menu (under Network), or run `~/.local/bin/tailswitch`.
4. Optionally turn on **Start at login** in the tray menu.

To remove it, run `./uninstall.sh` from the same folder. It stops the tray, deletes the installed files and the autostart entry, and leaves Tailscale untouched.

## Using the tray

```text
TailSwitch · Connected
Status view only · use the CLI to connect or disconnect
────────────────────────
This device      >  steamdeck · 100.x.y.z
Devices (3)      >  laptop · 100.x.y.z
                    server · 100.x.y.z
                    phone · 100.x.y.z · Offline
Click a device to copy its IPv4
────────────────────────
Refresh now
Status details…
☐ Start at login
About…
Quit
```

Copying goes through Plasma's clipboard manager (Klipper), which must be running. The menu confirms each copy quietly; a failed copy shows a notification instead of pretending it worked.

The tray icon requests attention only when you need to act: sign-in required, device approval required, Tailscale in use by another user, or status unreadable. Ordinary Tailscale health warnings stay in the menu header and **Status details** without pulsing the icon.

## Troubleshooting

- **"Tailscale CLI not found"**: install Tailscale, or launch with `tailswitch --tailscale-path /absolute/path/to/tailscale`.
- **"Status unavailable"**: open **Status details** for the reason. Common causes are a stopped `tailscaled` service, an expired login, or missing permission to read status. TailSwitch keeps retrying automatically.
- **Copy failed**: Plasma's Clipboard manager is not running. Check the system tray's Clipboard entry in Desktop Mode.
- **No tray icon**: TailSwitch needs a Plasma session with a system tray. It exits with code 2 when none is available, so it does nothing in Gaming Mode.
- **Diagnostics without touching your clipboard**:

  ```sh
  tailswitch --smoke-test     # tray and library check, exits after 1.5 s
  tailswitch --check-status   # prints only state and counts, never names or addresses
  ```

## Building from source

TailSwitch is C++17 with Qt 6 Widgets, KDE Frameworks' StatusNotifierItem, and CMake. Build it in a container or on any system with Qt >= 6.8 and KF6StatusNotifierItem >= 6.14:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./packaging/make-release.sh   # builds, tests, and writes dist/tailswitch-<version>-linux-x86_64.tar.gz
```

See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for the Steam Deck Distrobox setup, the test suite, manual checks, and the release process. Compatibility notes live in [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md).

## Contributing

Issues and pull requests are welcome. Please keep real tailnet data out of the repository: test fixtures must be synthetic, and bug reports should not include device names, addresses, login URLs, or raw `tailscale status` output.

## License

TailSwitch's code is licensed under the [MIT License](LICENSE). It links dynamically against the Qt and KDE Frameworks libraries already present on SteamOS and does not redistribute them.

The Tailscale name and logo are trademarks of Tailscale Inc., used here only to identify the software TailSwitch works with. The logo is not covered by the MIT license; see [assets/README.md](assets/README.md).

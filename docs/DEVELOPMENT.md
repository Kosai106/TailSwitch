# Development, testing, and releasing

## Build environment

SteamOS ships no compiler, so build in the `tailswitch-kde-dev` Distrobox container (Ubuntu 26.04). Create it once:

```sh
distrobox create --name tailswitch-kde-dev --image docker.io/library/ubuntu:26.04
distrobox enter tailswitch-kde-dev -- sudo apt-get update
distrobox enter tailswitch-kde-dev -- sudo apt-get install -y build-essential cmake ninja-build git pkg-config dbus-daemon qt6-base-dev qt6-base-dev-tools qt6-svg-plugins libkf6statusnotifieritem-dev extra-cmake-modules
```

`qt6-svg-plugins` provides the Qt SVG icon engine that renders the embedded tray logo at runtime; it is a plugin, not a link-time dependency, so a build without it succeeds but the icon tests fail and the tray icon would be blank. SteamOS ships it as `qt6-svg`. CMake requires Qt >= 6.8 and KF6StatusNotifierItem >= 6.14 (for `setIsMenu`). The container provides Qt 6.10.2 and KF6 6.24.0; the CI workflow uses the same image so shipped binaries match.

Runtime compatibility works in one direction: a binary built against Qt 6.10 needs Qt >= 6.10 on the host (the executable carries `Qt_6.10` symbol versions). It needs only glibc >= 2.34. Building in a container with a newer Qt than SteamOS would produce a binary that fails to load, so keep the container's Qt at or below the SteamOS stable release you intend to support.

## Build and test

From the host:

```sh
cd /home/deck/Developer/TailSwitch
distrobox enter tailswitch-kde-dev -- cmake -S "$PWD" -B "$PWD/build/kde-dev" -G Ninja -DCMAKE_BUILD_TYPE=Debug
distrobox enter tailswitch-kde-dev -- cmake --build "$PWD/build/kde-dev"
distrobox enter tailswitch-kde-dev -- ctest --test-dir "$PWD/build/kde-dev" --output-on-failure
```

Run the tests inside the container so they use its matching Qt Test library; the application itself does not link Qt Test.

### Automated checks

CTest runs:

- **tailscale_status_and_client:** synthetic JSON parsing, known backend states, invalid structures, online-first sorting, IPv4 selection, absent fields, fragmented output, error classification, crashes/timeouts/output limits, executable discovery, paths with spaces, request coalescing, recovery, and periodic refresh through a fake CLI.
- **desktop_behaviors:** embedded/cropped application icon, native tray-menu export/ownership, fake clipboard acknowledgement/failure, live device-menu reconciliation, offline copying, missing-IPv4 disabling, stale-action invalidation, ordinary-health non-pulsing behavior, repeated-error notification suppression, autostart entry lifecycle, and the **Start at login** menu toggle.
- **no_copy_relocations:** inspects the executable's ELF relocations to prevent a known SteamOS Qt loader failure (see below).
- **check_status_summary:** runs the one-shot checker against the fake CLI and rejects fixture names, IPs, health text, or authentication markers in its output.
- **cli_help:** verifies command-line help without a desktop session.

All fixtures are synthetic. The fake CLI refuses any arguments except `status --json`; it never calls real Tailscale. Desktop tests use fake StatusNotifierWatcher, Klipper, and notification services under a private `dbus-run-session`, never the real tray, clipboard, or notifications. Autostart tests write into a temporary directory, never `~/.config/autostart`.

### Why `-fPIC`

SteamOS's Qt exports protected data symbols that reject ELF copy relocations. The desktop library and everything linking it are compiled with `-fPIC`; default PIE alone was insufficient and produced a loader error about `QByteArray::_empty`. The `no_copy_relocations` test guards this.

## Manual checks on the Deck

Run these on the **host**, not inside the container.

Tray and library check (no Tailscale or clipboard access, exits after about 1.5 s):

```sh
QT_QPA_PLATFORM=wayland ./build/kde-dev/tailswitch --smoke-test
```

Exit code 2 means no tray was available. With `QT_QPA_PLATFORM=offscreen` it should report no tray and exit 2 rather than linger invisibly.

Read-only CLI check, printing only state and counts:

```sh
QT_QPA_PLATFORM=offscreen ./build/kde-dev/tailswitch --check-status
```

Live run with a synthetic tailnet, without querying yours (clicking still changes your real clipboard):

```sh
TAILSWITCH_FAKE_SCENARIO=success QT_QPA_PLATFORM=wayland \
  ./build/kde-dev/tailswitch --tailscale-path "$PWD/build/kde-dev/fake_tailscale"
```

Live run against your real Tailscale:

```sh
QT_QPA_PLATFORM=wayland ./build/kde-dev/tailswitch
```

Then walk through:

1. Left-click and right-click both open Plasma's menu without a Wayland grabbing-popup warning.
2. **This device** and **Devices** show the expected IPv4 addresses; online peers sort first; offline peers are copyable; peers without IPv4 are disabled.
3. Clicking a device replaces the clipboard text with its IPv4 and shows quiet confirmation.
4. Keeping the device submenu open across the 10-second refresh does not rebuild it.
5. **Status details** shows guidance, the CLI version, last read time, and health messages. Ordinary health warnings do not pulse the icon.
6. **Start at login** writes `~/.config/autostart/tailswitch.desktop` when checked and removes it when unchecked. The entry launches the executable you toggled it from, so toggle it from an installed copy, not a build directory, before relying on it.
7. Launching a second copy prints "already running" and exits 0 without adding a tray icon.
8. **About** and **Quit** behave; closing dialogs does not exit; quitting does not disconnect Tailscale.

Do not disconnect Tailscale, stop its daemon, or change operator permissions just to test failure handling. Use the fake CLI scenarios (`hang`, `crash`, `flood`, `permission`, `daemon`, `login-text`, `failure`, `bad-json`, `bad-schema`, `stopped`, `login`, `fragmented`) instead.

Copying uses Klipper's session-bus API because a menu exported to Plasma can trigger actions without giving this process a Wayland input serial, which makes `QClipboard` writes silently ineffective. Success is the service's acknowledgement, not an independent paste check; the app never reads clipboard contents or history.

## Installing a local build

The release archive's `install.sh` is the supported path. For a quick local install of a development build:

```sh
distrobox enter tailswitch-kde-dev -- cmake --install "$PWD/build/kde-dev" --prefix "$HOME/.local"
```

The installed desktop entry uses `Exec=tailswitch`, which requires `~/.local/bin` on the launcher's PATH. The release installer instead rewrites `Exec` to the absolute path, so prefer it for anything you keep around.

## Releasing

The repository ships a Claude Code skill, `/release` (`.claude/skills/release/SKILL.md`), that walks through this process step by step, including the Deck checks and the release-page cleanup. The short version:

1. Update the version in `CMakeLists.txt` (`project(TailSwitch VERSION x.y.z ...)`) and add a `CHANGELOG.md` entry with the date.
2. Build and test the archive locally:

   ```sh
   distrobox enter tailswitch-kde-dev -- ./packaging/make-release.sh
   ```

   This configures `build/release` as a Release build, runs CTest, strips the binary, and writes `dist/tailswitch-x.y.z-linux-x86_64.tar.gz` plus a SHA-256 file. The archive contains the binary, desktop entry, icon, install/uninstall scripts, license, changelog, and trademark notice.
3. Test the archive on the Deck: extract it, run `./install.sh`, launch from the application menu, toggle **Start at login**, log out and back in, then `./uninstall.sh`. To dry-run the installer without touching your real home, run it with `HOME` pointed at a scratch directory.
4. Commit, tag `vx.y.z`, and push the tag. The GitHub Actions workflow rebuilds the archive in an `ubuntu:26.04` container, checks that the tag matches the CMake version, and attaches the archive and checksum to a GitHub release.

The shipped binary links dynamically against SteamOS's own Qt and KDE Frameworks libraries and does not redistribute them, so no third-party library notices ship in the archive. If a future release bundles Qt or KDE libraries, add their license notices and satisfy the LGPL's relinking requirements before publishing.

# Development and smoke testing

## Build in Distrobox

Use `tailswitch-kde-dev` (Ubuntu 26.04), with `build-essential`, `cmake`, `ninja-build`, `qt6-base-dev`, `qt6-base-dev-tools`, `libkf6statusnotifieritem-dev`, `extra-cmake-modules`, `git`, `pkg-config`, and `dbus-daemon`.

CMake requires Qt >= 6.8 and KF6StatusNotifierItem >= 6.14; installed framework versions can impose higher transitive Qt requirements. The verified container has Qt 6.10.2 and KF6StatusNotifierItem 6.24.0.

From the host:

```sh
cd /home/deck/Developer/TailSwitch
distrobox enter tailswitch-kde-dev -- cmake -S "$PWD" -B "$PWD/build/kde-dev" -G Ninja -DCMAKE_BUILD_TYPE=Debug
distrobox enter tailswitch-kde-dev -- cmake --build "$PWD/build/kde-dev"
distrobox enter tailswitch-kde-dev -- ctest --test-dir "$PWD/build/kde-dev" --output-on-failure
```

Do not reuse `build/dev`: it contains the old Ubuntu 24.04 CMake cache and executable. The old `tailswitch-dev` container was removed after successful user validation; see the [migration record](TODO.md).

## Automated checks

CTest runs:

- **tailscale_status_and_client:** synthetic JSON parsing, known backend states, invalid structures, online-first sorting, IPv4 selection, absent fields, fragmented output, error classification, crashes/timeouts/output limits, executable discovery, paths with spaces, request coalescing, recovery, and periodic refresh through a fake CLI.
- **desktop_behaviors:** embedded/cropped application icon, native tray-menu export/ownership, fake clipboard acknowledgement/failure, live device-menu reconciliation, offline copying, missing-IPv4 disabling, stale-action invalidation, ordinary-health non-pulsing behavior, and repeated-error notification suppression.
- **no_copy_relocations:** inspect the executable's ELF relocations to prevent a known SteamOS Qt loader failure.
- **check_status_summary:** run the full app's one-shot checker against the fake CLI and reject fixture names, IPs, health text, or authentication markers in its output.
- **cli_help:** verify command-line help without a desktop session.

All fixtures are synthetic. The fake CLI refuses any arguments except `status --json`; it never calls real Tailscale. Desktop tests use fake StatusNotifierWatcher, Klipper, and notification services under a private `dbus-run-session`, never the real tray/clipboard/notifications. The tray test reads the actual exported `ItemIsMenu` and `Menu` properties and checks the D-Bus menu interface. Merely observing `QMenu::aboutToShow` or offscreen visibility cannot prove Wayland accepted a popup grab, so those are no longer used as success criteria.

Run automated tests inside Distrobox against its matching Qt Test version. The earlier Qt 6.4 test executable failed against host Qt 6.11 due to a missing Qt Test internal symbol; the application does not link Qt Test.

## Host runtime check

Run on the **host**, not inside Distrobox, to check SteamOS's runtime libraries:

```sh
QT_QPA_PLATFORM=wayland ./build/kde-dev/tailswitch --smoke-test
```

This reports compile/runtime Qt versions, platform, tray availability, and native menu-only configuration. It briefly shows the icon and exits after approximately 1.5 seconds. It does not access Tailscale or change the clipboard. Exit code 2 means a required tray capability was unavailable. A successful exit does not prove visual rendering or clipboard transfer.

The host D-Bus contract can also be inspected while the app runs: `/StatusNotifierItem` must export `org.kde.StatusNotifierItem.ItemIsMenu = true`, and its `Menu` property must reference a valid `com.canonical.dbusmenu` object. On the verified host, that object is `/MenuBar`. The item can use a separate unique bus connection from the app's main connection; do not assume a fixed well-known service name. Use `busctl --user list` to find the app's connections and introspect them.

Do **not** simulate primary activation by calling `Activate` and expect a local popup: menu-only items tell Plasma to present the exported menu instead.

## Read-only CLI check

```sh
QT_QPA_PLATFORM=offscreen ./build/kde-dev/tailswitch --check-status
# If the installed CLI is not found automatically:
QT_QPA_PLATFORM=offscreen ./build/kde-dev/tailswitch --check-status --tailscale-path /opt/tailscale/tailscale
```

This reads `tailscale status --json` once, printing only connection state, peer count, local-IPv4 availability, and health-message count. Exit 0 means a supported status was read, not necessarily that networking is connected. Failure prints a generic actionable message and exits 1. Device names/addresses, raw errors, health text, and authentication URLs are not printed.

Normal mode locates the CLI using PATH plus `/opt/tailscale/tailscale`, `/usr/local/bin/tailscale`, and `/usr/bin/tailscale`. An explicit path must be absolute and executable; a bad override is not silently ignored. The adapter uses no shell, normalizes CLI error language with `LC_ALL=C`, and does not execute `up`, `down`, `set`, login, or debug-preference commands.

## Manual live tray and clipboard check

Quit any old instance first, then run:

```sh
QT_QPA_PLATFORM=wayland ./build/kde-dev/tailswitch
```

1. Find the Tailscale nine-dot logo in the tray. It is embedded in the executable, cropped to the artwork with a small anti-aliasing margin, and tinted using the application's current foreground palette for light/dark legibility.
2. Left-click and right-click the icon separately. Both should open Plasma's menu without a Wayland grabbing-popup warning.
3. Confirm **This device** and **Devices** show the expected Tailscale IPv4 addresses. Online peers sort first; offline peers remain visible and copyable. Peers with no IPv4 are disabled, rather than copying an endpoint, route, IPv6, or empty text.
4. Click a device to replace your clipboard text with its IPv4, then paste into a text editor. Check quiet success feedback. Repeat from both click paths and, when available, with an offline peer.
5. Keep the device submenu open across the 10-second refresh interval. Unchanged actions should retain their identity rather than being cleared/rebuilt. Check **Refresh now** too.
6. Open **Status details**. It shows guidance for the reported backend state, last successful read time, CLI version, and health messages as local plain text. Connected status is not an end-to-end connectivity diagnosis. Ordinary health messages may add “Health warning” to the menu header but must not pulse the icon. Login/approval/other-user states and unreadable status still request attention.
7. Check **About**, then **Quit**. Closing dialogs must not exit the app; quitting must not disconnect Tailscale.
8. Record regressions in [follow-ups](TODO.md). Native menus, old-container cleanup, and the real peer/copy workflow have been confirmed; the revised icon still needs visual confirmation.

Do not disconnect Tailscale, stop its daemon, or alter operator permissions just to test failure handling without explicit approval. Use the fake CLI for those tests.

For a synthetic UI preview without querying your tailnet:

```sh
TAILSWITCH_FAKE_SCENARIO=success QT_QPA_PLATFORM=wayland \
  ./build/kde-dev/tailswitch --tailscale-path "$PWD/build/kde-dev/fake_tailscale"
```

The fake executable is a developer-test artifact backed by `tests/fixtures`. Its IPs are synthetic; clicking still explicitly changes your real clipboard.

Copying uses KDE's Klipper session-bus API. Plasma's Clipboard manager must be running. The action is disabled while its bounded request is pending; service acknowledgement shows quiet success. Failure/timeout re-enables the action, marks failure in the menu, and requests an error notification. The app does not read clipboard contents/history or fall back to an ineffective unfocused clipboard write.

In normal mode, device data is real and held in memory only. Status polls run every 10 seconds, with no overlapping requests, a 5-second timeout, and an 8 MiB combined retained-output cap. Failed reads invalidate copy actions until recovery; identical consecutive failures produce only one requested notification. Health messages are visible in Status details, not logged or automatically copied.

Connection/exit-node controls, autostart, explicit suspend/resume integration, real failure-notification delivery, and distributable packaging remain unimplemented or unvalidated. Periodic polling resumes with the event loop, but immediate resume handling still needs work.

For missing-tray handling, `QT_QPA_PLATFORM=offscreen ./build/kde-dev/tailswitch --smoke-test` should report no tray and exit 2, rather than linger invisibly.

## Runtime dependencies

The development executable dynamically links Qt and KDE libraries; it is not a portable release. The host needs KF6StatusNotifierItem >= 6.14 for menu-only support, plus compatible transitive dependencies.

The new Ubuntu toolchain initially produced an executable that failed against SteamOS Qt with a protected `QByteArray::_empty` symbol/copy-relocation error. Compiling the desktop library and its executable/test consumers with `-fPIC` avoids direct external-data access through ELF copy relocations. Default PIE alone was insufficient. Keep the CTest relocation guard.

Successful execution on this Deck does not establish compatibility with other SteamOS versions. Do not copy container Qt plugins into the host Qt installation or mix plugin/library versions. Release packaging and Qt/KDE dependency licensing will be validated separately.

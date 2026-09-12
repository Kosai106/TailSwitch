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

Do not reuse `build/dev`: it contains the old Ubuntu 24.04 CMake cache and executable. The old `tailswitch-dev` container is retained temporarily; see the [cleanup reminder](TODO.md).

## Automated checks

CTest runs:

- **desktop_behaviors:** six Qt Test cases for native tray menu export, ownership/no app-owned popup, acknowledged clipboard writes, rejected writes, missing service, and empty text.
- **no_copy_relocations:** inspect the executable's ELF relocations to prevent a known SteamOS Qt loader failure.
- **cli_help:** verify command-line help without a desktop session.

Desktop tests use a fake StatusNotifierWatcher and fake Klipper under a private `dbus-run-session`, never the real tray/clipboard. The tray test reads the actual exported `ItemIsMenu` and `Menu` properties and checks the D-Bus menu interface. Merely observing `QMenu::aboutToShow` or offscreen visibility cannot prove Wayland accepted a popup grab, so those are no longer used as success criteria.

Run automated tests inside Distrobox against its matching Qt Test version. The earlier Qt 6.4 test executable failed against host Qt 6.11 due to a missing Qt Test internal symbol; the application does not link Qt Test.

## Host runtime check

Run on the **host**, not inside Distrobox, to check SteamOS's runtime libraries:

```sh
QT_QPA_PLATFORM=wayland ./build/kde-dev/tailswitch --smoke-test
```

This reports compile/runtime Qt versions, platform, tray availability, advertised notification support, and native menu-only configuration. It briefly shows the icon and exits after approximately 1.5 seconds. It does not access Tailscale or change the clipboard. Exit code 2 means a required tray capability was unavailable. A successful exit does not prove visual rendering or clipboard transfer.

The host D-Bus contract can also be inspected while the app runs: `/StatusNotifierItem` must export `org.kde.StatusNotifierItem.ItemIsMenu = true`, and its `Menu` property must reference a valid `com.canonical.dbusmenu` object. On the verified host, that object is `/MenuBar`. The item can use a separate unique bus connection from the app's main connection; do not assume a fixed well-known service name. Use `busctl --user list` to find the app's connections and introspect them.

Do **not** simulate primary activation by calling `Activate` and expect a local popup: menu-only items tell Plasma to present the exported menu instead.

## Manual tray and clipboard check

Quit any old instance first, then run the **new path**:

```sh
QT_QPA_PLATFORM=wayland ./build/kde-dev/tailswitch
```

1. Find the blue connected-dots icon in the tray (possibly in Plasma's hidden-icons area).
2. Left-click to open the menu. Verify placement, focus, keyboard navigation, and legibility at your normal display scaling. There should be no Wayland grabbing-popup warning.
3. Dismiss it, then right-click; both paths should present Plasma's exported menu, not an app-owned popup.
4. Open **Sample devices (synthetic)** and choose **Copy sample IPv4**. This explicitly replaces clipboard text with `100.64.0.1`; it is not a real peer address.
5. Paste into a text editor and verify the value. Reopen the menu to see quiet copy feedback. Repeat from both left-click and right-click menus.
6. Try **About**, close the dialog, and confirm the tray app stays running.
7. Choose **Quit** and confirm the icon disappears. Optionally test whether Plasma retains the copied value after exit.
8. After confirming these checks, return to the [old-container cleanup reminder](TODO.md).

Copying uses KDE's Klipper session-bus API. Plasma's Clipboard manager must be running. The action is disabled while its bounded request is pending; service acknowledgement shows quiet success. Failure/timeout re-enables the action, marks failure in the menu, and requests an error notification. The app does not read clipboard contents/history or fall back to an ineffective unfocused clipboard write.

All device data is synthetic. Notifications, autostart, suspend/resume, real Tailscale access, and distributable packaging remain unvalidated.

For missing-tray handling, `QT_QPA_PLATFORM=offscreen ./build/kde-dev/tailswitch --smoke-test` should report no tray and exit 2, rather than linger invisibly.

## Runtime dependencies

The development executable dynamically links Qt and KDE libraries; it is not a portable release. The host needs KF6StatusNotifierItem >= 6.14 for menu-only support, plus compatible transitive dependencies.

The new Ubuntu toolchain initially produced an executable that failed against SteamOS Qt with a protected `QByteArray::_empty` symbol/copy-relocation error. Compiling the desktop library and its executable/test consumers with `-fPIC` avoids direct external-data access through ELF copy relocations. Default PIE alone was insufficient. Keep the CTest relocation guard.

Successful execution on this Deck does not establish compatibility with other SteamOS versions. Do not copy container Qt plugins into the host Qt installation or mix plugin/library versions. Release packaging and Qt/KDE dependency licensing will be validated separately.

# Development and smoke testing

## Build in Distrobox

The `tailswitch-dev` Ubuntu 24.04 container needs `build-essential`, `cmake`, `ninja-build`, `qt6-base-dev`, `qt6-base-dev-tools`, `git`, and `pkg-config`.

From the host:

```sh
cd /home/deck/Developer/TailSwitch
distrobox enter tailswitch-dev -- cmake -S "$PWD" -B "$PWD/build/dev" -G Ninja -DCMAKE_BUILD_TYPE=Debug
distrobox enter tailswitch-dev -- cmake --build "$PWD/build/dev"
distrobox enter tailswitch-dev -- ctest --test-dir "$PWD/build/dev" --output-on-failure
```

CTest runs CLI help and six desktop behavior checks: primary-click menu opening, other activation handling, acknowledged clipboard writes, rejected writes, missing service, and empty text. Clipboard tests use a fake Klipper service under a private `dbus-run-session`, never the real clipboard. Offscreen popup tests may report expected keyboard-grab/raise warnings.

Run tests inside Distrobox against the matching Qt Test version. The Qt 6.4-built test executable fails against the host's Qt 6.11 Test library due to a missing Qt Test internal symbol; the application itself does not link Qt Test and passes the host smoke check.

## Host runtime check

Run on the **host**, not inside Distrobox, to check the build against SteamOS's Qt runtime and platform plugins:

```sh
QT_QPA_PLATFORM=wayland ./build/dev/tailswitch --smoke-test
```

This reports compile/runtime Qt versions, platform, tray availability, and advertised notification support. It briefly shows the tray icon and exits after approximately 1.5 seconds. It does not access Tailscale or change the clipboard. Exit code 2 means no tray was available; a successful exit does not prove visual rendering or clipboard transfer.

## Manual tray and clipboard check

```sh
QT_QPA_PLATFORM=wayland ./build/dev/tailswitch
```

1. Find the blue connected-dots icon in the tray (possibly in Plasma's hidden-icons area).
2. Left-click the icon to open the menu; verify placement and legibility at your normal display scaling. Dismiss it, then right-click and check the platform-provided context menu too.
3. Open **Sample devices (synthetic)** and choose **Copy sample IPv4**. This explicitly replaces your clipboard text with `100.64.0.1`; it is not a real peer address.
4. Paste into a text editor and verify the value. Reopen the tray menu to see quiet copy feedback. Repeat from both the left-click menu and right-click menu; their focus behavior differs on Wayland.
5. Try **About**, close the dialog, and confirm the tray app stays running.
6. Choose **Quit** and confirm the icon disappears. Optionally test pasting again to learn whether Plasma retains the clipboard after the source exits.

Copying uses KDE's Klipper session-bus API, not a focus-dependent Qt clipboard write. The host must have Plasma's Clipboard manager running. While a request is pending, the copy action is disabled; an acknowledgement shows quiet success feedback. Failure/timeout re-enables the action, marks failure in the menu, and requests an error notification. There is no silent fallback to a potentially ineffective unfocused clipboard write. The app does not read clipboard contents or history.

All displayed device data is synthetic. No connect/disconnect or settings actions exist yet. Notifications, autostart, suspend/resume, actual Tailscale access, and distributable packaging remain untested.

For a missing-tray check, `QT_QPA_PLATFORM=offscreen ./build/dev/tailswitch --smoke-test` should report no system tray and exit with code 2 instead of lingering invisibly.

## Runtime dependencies

The development executable is dynamically linked and is not a portable release. Successful execution against host Qt does not establish compatibility with other SteamOS versions. Do not copy Ubuntu's Qt plugins into the host Qt installation or mix plugin/library versions. Release packaging will be designed and validated separately.

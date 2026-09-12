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

The current CTest check only verifies command-line help in Qt's offscreen platform. Behavioral tests will accompany the CLI adapter and application state model.

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
2. Open its context menu and verify it is legible at your normal display scaling.
3. Open **Sample devices (synthetic)** and choose **Copy sample IPv4**. This explicitly replaces your clipboard text with `100.64.0.1`; it is not a real peer address.
4. Paste into a text editor and verify the value. Reopen the tray menu to see quiet copy feedback.
5. Try **About**, close the dialog, and confirm the tray app stays running.
6. Choose **Quit** and confirm the icon disappears. Optionally test pasting again to learn whether Plasma retains the clipboard after the source exits.

All displayed device data is synthetic. No connect/disconnect or settings actions exist yet. Notifications, autostart, suspend/resume, actual Tailscale access, and distributable packaging remain untested.

For a missing-tray check, `QT_QPA_PLATFORM=offscreen ./build/dev/tailswitch --smoke-test` should report no system tray and exit with code 2 instead of lingering invisibly.

## Runtime dependencies

The development executable is dynamically linked and is not a portable release. Successful execution against host Qt does not establish compatibility with other SteamOS versions. Do not copy Ubuntu's Qt plugins into the host Qt installation or mix plugin/library versions. Release packaging will be designed and validated separately.

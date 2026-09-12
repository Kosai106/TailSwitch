# Compatibility spike

## Observed development environment

Local observations, not a complete supported-version matrix:

| Item | Finding |
| --- | --- |
| Project root | `/home/deck/Developer/TailSwitch` |
| OS | SteamOS 3.10, steamdeck variant |
| Desktop | KDE, Wayland session |
| Host Qt | 6.11.1 |
| Host KF6StatusNotifierItem / WindowSystem | 6.28.0 |
| Tailscale CLI | `/opt/tailscale/tailscale`, version 1.102.3 |
| Host build tools | Compiler/CMake not installed; keep SteamOS base untouched |
| Active build environment | `tailswitch-kde-dev`, Ubuntu 26.04 under Distrobox |
| Active toolchain | CMake 4.2.3, GCC 15.2.0, Qt 6.10.2, KF6StatusNotifierItem 6.24.0 |
| Retired environment | `tailswitch-dev`, Ubuntu 24.04, Qt 6.4.2; removed after successful user validation |
| Container engine | Podman 6.0.2; builds verified |
| Host ABI | x86_64, glibc 2.43 |
| Git | 2.55.0; main branch; user-configured author identity |

CMake requires Qt >= 6.8 and KF6StatusNotifierItem >= 6.14, subject to framework package transitive requirements. Only the versions above have been tested. CLI discovery from a desktop launcher may differ from a shell.

## Verified from installed Tailscale CLI help

- `tailscale status --json` exists; help warns that its format may change across releases.
- `tailscale set` changes only explicitly specified preferences.
- `set` supports `--operator`, `--exit-node`, and `--exit-node-allow-lan-access`.
- An explicit empty exit-node argument disables exit-node use. Pass it as a QProcess argument, not shell text.
- `tailscale up` without flags reconnects without changing settings. Do not use preference-setting flags or `--reset` for reconnect.
- `up` can initiate authentication; gate reconnect on existing authentication and handle login-required transitions without a v1 sign-in workflow.

No mutating Tailscale commands have been run. Read-only `status --json` and `debug prefs` succeeded without elevation on this Deck. Inspection emitted only field names/types, backend state/counts, and selected preference booleans/configured-presence indicators; real names, addresses, authentication URLs, and preference dumps were not saved or printed.

The operator preference is configured, but read access and a nonempty preference do not establish that this user has mutation rights. Verify those before implementing controls. `debug prefs` is explicitly an unstable interface; its selected exit-node/LAN-access fields are available here, but the running app does not use this command yet.

## Desktop findings and corrections

### Clipboard

The first QClipboard-based implementation failed in user testing. A menu exported to Plasma can invoke app actions without giving the app a Wayland input serial/focused surface.

Host introspection confirmed `org.kde.klipper.klipper.setClipboardContents(s)` at `org.kde.klipper`, `/klipper`. The implementation now calls that API asynchronously with a 3-second timeout, acknowledges success only after a successful reply, and reports errors without a silent Qt fallback. It never reads clipboard/history contents.

**User confirmed that copying works** after this change and subsequently confirmed the native tray replacement works after being asked to recheck copying and both click paths. Clipboard code is unchanged. Automated tests only use a fake clipboard service.

### Left-click tray menus

The initial QSystemTrayIcon menu worked on right-click only. Wiring its primary activation to `QMenu::popup()` then appeared successful in offscreen tests and an activation smoke check, but user testing revealed the Wayland error:

> Failed to create grabbing popup ... transientParent ... parent window has received input.

The earlier smoke result only established that a popup was requested, not that the compositor granted its grab. It was insufficient evidence of functional left-click behavior.

The replacement uses **KStatusNotifierItem::setIsMenu(true)** (introduced in KDE Frameworks 6.14). Plasma reads `ItemIsMenu=true` and presents the exported D-Bus menu for primary click as well as right-click. There is no custom activation callback, fake focus window, or app-owned `QMenu::popup()` path. KStatusNotifierItem owns its heap-allocated menu.

Ubuntu 24.04 does not provide the needed KF6 development package. The user provisioned Ubuntu 26.04 as a separate container. After the user confirmed the replacement works, the old container was removed; the new container and shared repository were preserved.

### New-container validation

- Configure and compilation succeeded in `tailswitch-kde-dev` without compiler warnings.
- Initial host startup failed on an ELF copy relocation against protected `QByteArray::_empty` in host Qt. Adding `-fPIC` to the desktop library and executable/test consumers corrected this; default Ubuntu PIE alone was insufficient.
- CTest passed **3/3**: native desktop/clipboard behavior tests, ELF copy-relocation guard, and CLI help.
- Desktop tests use a fake tray watcher and clipboard service on a private D-Bus session. They verify actual exported `ItemIsMenu`/`Menu` properties and the menu interface, not offscreen popup visibility.
- Host Wayland smoke check exited 0 using container-built Qt 6.10.2 code against host Qt 6.11.1 and KF6 6.28.0.
- Live host introspection confirmed `ItemIsMenu = true`, `Menu = /MenuBar`, and the `com.canonical.dbusmenu` interface with `GetLayout`, `Event`, and `AboutToShow` methods.
- `ldd` resolved Qt and KDE libraries from the host with no missing dependencies reported.
- Offscreen/missing-tray check exited 2 with an actionable diagnostic.
- User confirmed the replacement works after being asked to verify left-click, right-click, and clipboard behavior. This supplements the protocol checks; comprehensive scaling and keyboard-navigation coverage remain release checks.

Run automated tests in the matching container runtime. The old Qt 6.4 test binary failed against host Qt 6.11 due to a missing Qt Test internal symbol; the application itself does not link Qt Test.

## Read-only milestone validation

- The running app executes only `tailscale status --json`, with QProcess executable/argument separation and no shell. Discovery supports PATH and SteamOS's `/opt/tailscale/tailscale`, plus an explicit absolute-path override.
- Status refreshes every 10 seconds, supports manual refresh, and never overlaps child queries. Reads have a 5-second timeout and an 8 MiB combined retained-output limit. Quitting only terminates an outstanding read process, not tailscaled.
- The parser handles the observed schema, known backend states, null/missing optional fields, unknown extra fields, IPv6-first lists, and deterministic online-first peer ordering. It rejects malformed required structures and unknown backend states rather than inventing state. Actual compatibility has been verified with Tailscale 1.102.3; a broader version matrix remains untested.
- Valid non-running JSON can be accepted with a nonzero CLI exit code. Crashes, nonzero running results, missing/broken executables, timeouts, excessive output, permission failures, and daemon errors are not presented as success.
- Device actions retain identity between unchanged refreshes. Removed/stale actions are disabled and cleared; failed reads invalidate device copying until recovery. Offline peers with IPv4 remain copyable; IPv6-only peers do not copy endpoint or route addresses.
- Health messages are displayed only in a local plain-text details dialog. Raw status/error output, peer identifiers, authentication URLs, and clipboard contents are never logged. `--check-status` prints only state/counts.
- CTest passed **5/5** entries, including synthetic parser/fake-CLI cases, UI/clipboard/notification tests on an isolated D-Bus session, output redaction, and the native-tray/ELF regressions.
- Live host `--check-status` successfully parsed the actual connected backend and peers. Host tray smoke check also passed. No real clipboard writes or networking mutations were performed by automated checks.
- User confirmed the real status/device/copy workflow works as intended. Explicit suspend/resume reconciliation and broader accessibility/scaling checks remain future work.

## Icon and attention-state polish

- The original generated blue/green/gray/amber line-and-node icon was replaced with the user-supplied Tailscale nine-dot SVG.
- The SVG's original `0 0 130 120` canvas placed approximately 53×53 units of artwork near its center. Its viewBox is now a square `37 32 57.04 57.04`, leaving roughly two source units around the artwork to avoid anti-aliasing clipping.
- The asset is embedded as a Qt resource, so runtime does not depend on the source SVG being installed beside the executable. The solid/translucent alpha pattern is retained and tinted to `QPalette::WindowText` at startup for light/dark application-theme legibility.
- A desktop test confirms the embedded icon renders and occupies at least 56×56 pixels of a 64×64 canvas, with a small nonzero margin. Host Wayland smoke startup continues to pass.
- The yellow pulse was KDE rendering `NeedsAttention`, previously set for any nonempty Tailscale health list. Routine health messages now remain visible in the menu header and Status details while the notifier stays `Active`. `NeedsAttention` is reserved for login, machine approval, another-user state, or inability to read status; its attention icon uses the same logo rather than separate yellow artwork.
- The logo is a Tailscale trademark and is explicitly excluded from the project's MIT grant in [the asset notice](../assets/README.md). Verify current brand/trademark requirements before public release.

## Remaining investigation

- [x] Get user confirmation that native left/right-click menus and clipboard work.
- [x] Remove the old `tailswitch-dev` container after successful validation; verified the replacement and shared project remain intact. See [follow-ups](TODO.md).
- [ ] Complete keyboard-navigation and scaling coverage for release.
- [ ] Check TailSwitch naming conflicts; check TailTray if fallback is needed.
- [x] Inspect status schema without persisting real tailnet data; construct synthetic fixtures.
- [x] Validate the real-device menu and copy workflow with the user.
- [ ] Validate the cropped, palette-tinted logo visually and confirm ordinary health messages no longer pulse.
- [ ] Finalize saved-preference reads for controls. Unprivileged `debug prefs` works locally, but its unstable API/versioning/privacy implications need explicit handling.
- [ ] Establish minimum supported Tailscale version and handling of unknown versions/fields.
- [ ] Validate operator detection, existing-operator handling, setup, and revocation instructions.
- [x] Test daemon-unavailable, login-required, and permission-denied handling with a fake CLI; live service-failure/expired-auth transitions remain untested.
- [ ] Validate real notification delivery and desktop-launched CLI discovery (PATH fallback is implemented).
- [ ] Test suspend/resume and external CLI state reconciliation.
- [ ] Validate home-folder installation, library/plugin discovery, launcher, autostart, and uninstall.
- [ ] Inventory bundled Qt/KDE libraries and satisfy redistribution requirements.

## Build environment setup

User-provisioned replacement:

```sh
distrobox create --name tailswitch-kde-dev --image docker.io/library/ubuntu:26.04
distrobox enter tailswitch-kde-dev -- sudo apt-get update
distrobox enter tailswitch-kde-dev -- sudo apt-get install -y build-essential cmake ninja-build git pkg-config dbus-daemon qt6-base-dev qt6-base-dev-tools libkf6statusnotifieritem-dev extra-cmake-modules
```

Build into `build/kde-dev`, not the old `build/dev` cache. See [development instructions](DEVELOPMENT.md).

The project lives in the shared home directory, outside either container's writable root. Package installation happens inside the container. Distrobox is a development convenience, not a security sandbox. Do not disable SteamOS read-only protection, change global Git identity, or remove the shared project directory when retiring a container.

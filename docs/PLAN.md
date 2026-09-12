# Approved v1 plan

## Product decisions

- Working name: TailSwitch; use TailTray if unavailable. Check both names before publication.
- Audience: public open-source project, MIT license for our code.
- Initial platform: SteamOS / Steam Deck, KDE Plasma Desktop Mode, including Wayland.
- UI: native Qt system tray with compact submenus, not a main application window.
- Stack: C++ / Qt 6 Widgets / CMake.
- Distribution: home-folder install, with dependencies bundled as needed; no system package changes.
- Existing Tailscale installation and login required. No browser sign-in in v1.
- One-time operator authorization explained with a copyable terminal command, no privileged GUI/helper.

## Menu and behavior

```text
TailSwitch · Connected
Disconnect
────────────────────────
This device             > Copy IPv4
Devices                 > laptop · 100.x.x.x
                          server · 100.x.x.x
                          phone · Offline
Exit node: None          > [selected] None
                            available-router
Allow local network access
────────────────────────
Settings…
About…
Quit
```

- Copy a peer's Tailscale IPv4 on click; provide quiet visual confirmation.
- Sort online peers first and clearly mark offline peers; offline addresses remain copyable.
- Handle missing IPv4 without copying an empty or unrelated address.
- Show current exit-node selection and availability. Do not offer an offline peer as a new selection.
- Preserve and flag a selected exit node if it becomes unavailable.
- Expose the exit-node LAN-access setting, without overwriting its saved value merely because it is temporarily inapplicable.
- Show connected, disconnected, pending, and attention-needed states.
- Provide opt-in launch at desktop login, disabled by default.
- Notify on errors; avoid repeated notifications for an unchanged failure.
- Disconnect uses `tailscale down`, not logout or daemon shutdown.
- Reconnect uses `tailscale up` without preference-setting flags; verify authentication state before offering it.
- Quitting, updating, or uninstalling the GUI leaves Tailscale networking unchanged.
- GUI autostart does not automatically connect or disconnect Tailscale.

## Architecture

Tray UI -> application state/action controller -> asynchronous CLI adapter -> existing tailscaled.

The adapter must:

- Discover the installed CLI, accounting for the desktop launcher's potentially different PATH.
- Use QProcess with explicit executable and argument lists, never shell interpolation.
- Read structured status JSON; tolerate optional fields and reject unsupported or malformed structures safely.
- Determine saved preferences through a verified read mechanism rather than inferring them from online peers.
- Change individual settings with `tailscale set`; never use `up --reset`.
- Serialize mutations, show pending state, enforce timeouts, and reconcile actual state after success or failure.
- Recognize that terminating a CLI process does not necessarily roll back a daemon-side change.
- Refresh periodically, after actions, and after suspend/resume; reflect changes made by external CLI users.
- Keep raw status/preferences and authentication information out of logs and committed fixtures.

Poll timing and supported minimum Tailscale/Qt versions will be established during the compatibility spike.

## Permissions and errors

Never run the GUI as root or store credentials. Operator setup offers an explained, copyable command such as:

```sh
sudo tailscale set --operator="$USER"
```

Explain that this grants the Linux user control of Tailscale, not exclusively this app. Detect existing setup, avoid unnecessarily replacing a different operator, and document revocation after verifying its behavior. Do not revoke authorization automatically on uninstall.

Distinguish missing CLI, stopped/unavailable daemon, insufficient permissions, login required/expired, command failure/timeout, and unsupported output. Offer instructions rather than silently performing setup.

When an exit node becomes unavailable, preserve selection, display a persistent warning, and issue one notification. Never automatically fall back to direct internet or another node. Describe reported reachability accurately; do not claim an end-to-end connectivity diagnosis from status alone.

## Deferred scope

- Browser sign-in and account switching.
- DNS and subnet-route acceptance controls.
- Shields-up, Tailscale SSH, and Taildrop.
- Advertising this device as an exit node.
- Automatic exit-node failover.
- Gaming Mode integration and support promises for other desktops.
- Automatic app updates, Flatpak, and AppImage packaging.

Windows/macOS desktop feature parity is a long-term direction, not a v1 release requirement.

## Milestones

1. **Compatibility spike:** verify CLI status/preferences, operator access, Qt tray/Wayland clipboard, build environment, and home-folder deployment.
2. **Read-only tray:** connection state, local address, sorted peers, copying, and actionable errors.
3. **Network controls:** connect/disconnect, exit-node selection, LAN access, serialized changes and timeout handling.
4. **Desktop integration:** setup guidance, notifications, autostart, settings persistence, suspend/resume recovery.
5. **Public release:** reproducible packaging, installer/uninstaller, documentation, dependency notices, tests, and project-name verification.

## Release acceptance

Use synthetic JSON fixtures and a fake CLI for deterministic testing. Perform integration tests on the actual Deck, requesting permission before disruptive networking tests.

- Existing DNS/routing preferences survive GUI actions.
- Failures and timeouts never appear as confirmed success.
- Unreachable exit nodes are not silently deselected or replaced.
- Offline peers remain copyable; absent IPv4 is handled clearly.
- External CLI changes appear in the UI.
- Suspend/resume recovers without stale controls or notification spam.
- Quit, update, and uninstall leave networking and login untouched.
- GUI works without root following operator setup.
- Installation requires no changes to SteamOS's read-only system.
- Wayland clipboard, tray menus, notifications, scaling, and autostart work in Desktop Mode.
- Shipping Qt and other libraries meets their licensing requirements.

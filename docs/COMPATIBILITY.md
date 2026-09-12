# Compatibility spike

## Observed development environment

These are local observations, not minimum supported versions or evidence of end-to-end compatibility.

| Item | Finding |
| --- | --- |
| Project root | `/home/deck/Developer/TailSwitch` |
| OS | SteamOS 3.10, steamdeck variant |
| Desktop | KDE, Wayland session |
| Qt | qmake6 reports 6.11.1; qt6-base and qt6-tools packages present |
| Tailscale CLI | `/opt/tailscale/tailscale`, version 1.102.3 |
| Host build tools | No cmake, ninja, make, gcc, g++, clang++, or pkg-config found on host PATH |
| Build environment | User provisioned `tailswitch-dev`, Ubuntu 24.04 under Distrobox |
| Container toolchain | CMake 3.28.3, GCC 13.3.0, Ninja 1.11.1, Qt 6.4.2 (Core/Gui/Widgets/Test) |
| Container engine | Podman 6.0.2; container builds verified |
| Host ABI | x86_64, glibc 2.43 |
| Git | 2.55.0; repository initialized on main; user-configured author identity verified |

Qt package presence does not prove development headers, CMake metadata, or all required runtime plugins are available. CLI discovery from a desktop launcher may differ from the current shell.

## Verified from installed CLI help

- `tailscale status --json` exists. Its help explicitly warns that the JSON format may change across releases.
- `tailscale set` changes only explicitly specified preferences.
- `set` offers `--operator`, `--exit-node`, and `--exit-node-allow-lan-access`.
- An empty exit-node argument disables exit-node use. Pass it as an explicit QProcess argument, not shell text.
- `tailscale up` with no flags reconnects without changing settings, according to its help. Preference-setting flags instead require the complete desired configuration; the app must not use them for reconnect.
- `up` can initiate authentication, so v1 must gate reconnect on existing authentication and handle login-required transitions rather than launching a sign-in workflow.

No mutating Tailscale command has been run. Actual daemon access, operator authorization, status JSON, and saved preferences have not yet been inspected.

## Remaining investigation

- [ ] Check TailSwitch naming conflicts; check TailTray if fallback is needed.
- [x] Select and validate a build environment that does not modify the SteamOS base system: Distrobox Ubuntu 24.04.
- [x] Verify development dependencies and initial host runtime ABI compatibility with a minimal executable. Release packaging compatibility remains separate.
- [ ] Inspect status schema without persisting real tailnet data; construct synthetic fixtures.
- [ ] Verify how to read saved exit-node and LAN-access preferences as an unprivileged user. Avoid relying on an undocumented/debug interface without recording versioning and privacy risks.
- [ ] Determine the minimum supported Tailscale version and behavior for unknown versions/fields.
- [ ] Validate operator permission detection, existing-operator handling, setup instructions, and revocation instructions.
- [ ] Verify daemon-unavailable, logged-out, expired-auth, and permission-denied detection.
- [ ] Build a minimal Qt tray and clipboard smoke test; verify KDE Wayland behavior, clipboard ownership, notifications, scaling, and missing-tray handling.
- [ ] Validate CLI discovery in a desktop-launched session.
- [ ] Test suspend/resume and external CLI state reconciliation.
- [ ] Validate home-folder installation, Qt plugin discovery, launcher, autostart, and uninstall.
- [ ] Inventory bundled libraries and satisfy Qt/dependency redistribution requirements.

## Prototype results

- CMake configure and compilation succeeded in Distrobox without compiler warnings.
- CMake reported missing optional XKB development files, but configuration/build completed. Revisit if later code or packaging needs them.
- CTest CLI-help check passed (1/1); this is not behavioral coverage of Tailscale features.
- Host smoke test exited 0 using native Wayland: compiled against Qt 6.4.2, running against host Qt 6.11.1.
- Qt reported a system tray available before and after event processing, and advertised notification support.
- `ldd` confirmed linkage to host Qt libraries under `/usr/lib` with no missing dependencies reported.
- Offscreen/missing-tray check exited 2 with an actionable diagnostic, without lingering invisibly.
- Actual visual tray/menu appearance, clipboard transfer, notification delivery, and scaling remain manual checks. See [development instructions](DEVELOPMENT.md).
- No Tailscale commands or automatic clipboard writes are part of the prototype.

## Build environment setup

The user provisioned Distrobox with Ubuntu 24.04, providing a stable toolchain and an older glibc baseline than this host. Initial host execution succeeded, but this does not establish distributability across SteamOS releases.

Setup used:

```sh
distrobox create --name tailswitch-dev --image docker.io/library/ubuntu:24.04
distrobox enter tailswitch-dev -- sudo apt-get update
distrobox enter tailswitch-dev -- sudo apt-get install -y build-essential cmake ninja-build qt6-base-dev qt6-base-dev-tools git pkg-config
```

The project remains at `/home/deck/Developer/TailSwitch`, accessible through Distrobox's shared home directory. Package installation above happens inside the container, not on the SteamOS host. Distrobox is a development convenience, not a security sandbox.

Next: manually validate tray appearance and clipboard transfer, then investigate read-only status/preferences and implement the tested CLI adapter. Do not disable SteamOS read-only protection or install host system packages as a shortcut.

The user configured the Git author identity before the first commit. Use that identity; do not invent one or change global Git configuration.

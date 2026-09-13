# Follow-ups

## 0.1.0 release checklist

Done in the repository:

- [x] Version 0.1.0 reported by `--version`, the About dialog, and the startup diagnostics.
- [x] Opt-in **Start at login** through an XDG autostart entry, with tests.
- [x] Single-instance guard via the `io.github.kosai106.TailSwitch` session-bus name; verified live on the Deck.
- [x] Desktop entry, icon, and CMake install rules.
- [x] Home-directory `install.sh`/`uninstall.sh`, verified against a scratch `HOME`; the desktop entry passes `desktop-file-validate`.
- [x] `packaging/make-release.sh` builds, tests, strips, and archives; verified in the container.
- [x] GitHub Actions workflow for CI and tagged releases.
- [x] Public README, changelog, and updated development/compatibility docs.
- [x] Name check: no Tailscale-related "TailSwitch" project found. The name is also used by an unrelated 2017 Perl log-tailing tool on CPAN (`App::tailswitch`), which does not conflict with a Tailscale tray.
- [x] Removed the stale `build/dev` artifacts from the retired container.

Needs the user on the Deck:

- [ ] Confirm the current tray logo is legible in the light and dark Breeze themes and no longer pulses for ordinary health messages.
- [ ] Extract the release archive, run `install.sh`, launch from the application menu, and check the launcher icon.
- [ ] Toggle **Start at login** from the installed copy, log out and back into Desktop Mode, and confirm exactly one tray icon appears.
- [ ] Push the branch and confirm the GitHub Actions build passes before tagging `v0.1.0`.
- [ ] Optional: add a tray/menu screenshot to the README.

## Known limitations to carry into 0.2

- The prebuilt binary needs Qt >= 6.10 at runtime (SteamOS 3.9 or newer). Supporting the SteamOS 3.8 stable channel means building against Qt 6.9 in a different container image; confirm the stable channel's Qt version first.
- Keyboard navigation and HiDPI scaling of the Plasma-rendered menu have not been checked systematically.
- Suspend/resume relies on the regular poll; there is no immediate refresh on resume.
- Real notification delivery has been exercised manually only for copy failures and status failures.

## Next development step

- [ ] Design targeted connect/disconnect and exit-node mutations, with verified saved-preference reads and operator permission handling (see [PLAN.md](PLAN.md)).
- [ ] Request permission before disruptive real-network integration tests. Keep fake-CLI tests as the default.
- [ ] Continue the remaining compatibility checks in [COMPATIBILITY.md](COMPATIBILITY.md).

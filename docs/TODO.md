# Follow-ups

## Build-container migration

- [x] Validate the `tailswitch-kde-dev` Ubuntu 26.04 build and automated tests (3/3 CTest entries passed).
- [x] Validate startup against SteamOS's host libraries and inspect native D-Bus menu export. Required `-fPIC` to avoid protected Qt data-symbol copy relocations; host smoke and protocol checks passed.
- [x] User confirmed the replacement works after being asked to verify left-click, right-click, and copying.
- [x] Removed the old `tailswitch-dev` Ubuntu 24.04 container with `distrobox rm --force tailswitch-dev` after confirmation. Verified it no longer exists and `tailswitch-kde-dev` remains running.

The shared project directory, new executable, unrelated containers, and container images were not removed. The old ignored `build/dev` artifacts remain; use `build/kde-dev` for all current builds and launches.

## Read-only milestone

- [x] Inspect local status schema and preference-read availability without saving real tailnet dumps.
- [x] Implement an asynchronous, bounded, tested status CLI adapter and typed status parser.
- [x] Display real connection status, local IPv4, and sorted peers with copying and quiet feedback.
- [x] Add periodic/manual refresh, actionable failures, health details, stale-action handling, and repeated-error suppression.
- [x] Validate the live host with the sanitized `--check-status` command; native tray smoke check passes.
- [x] User confirmed the real status/device/copy workflow works as intended.

## Icon polish

- [x] Replace generated colored connected-dots artwork with the supplied Tailscale nine-dot logo.
- [x] Crop the SVG from its 130×120 canvas to a square viewBox around the 53×53 artwork, retaining a small anti-aliasing margin.
- [x] Embed and palette-tint the SVG; test that its rendered alpha bounds fill the tray canvas rather than retaining the old whitespace.
- [x] Stop routine Tailscale health messages from setting KDE `NeedsAttention`; retain health text in the header/details.
- [ ] Get user confirmation that the logo is legible and no longer pulses for the currently reported health messages.

## Next development step

- [ ] Design targeted connect/disconnect and exit-node mutations, with verified saved-preference reads and operator permission handling.
- [ ] Request permission before disruptive real-network integration tests. Keep fake-CLI tests as the default.
- [ ] Continue the remaining compatibility and release checks in [COMPATIBILITY.md](COMPATIBILITY.md).

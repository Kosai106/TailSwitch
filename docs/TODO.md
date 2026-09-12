# Follow-ups

## Build-container migration

- [x] Validate the `tailswitch-kde-dev` Ubuntu 26.04 build and automated tests (3/3 CTest entries passed).
- [x] Validate startup against SteamOS's host libraries and inspect native D-Bus menu export. Required `-fPIC` to avoid protected Qt data-symbol copy relocations; host smoke and protocol checks passed.
- [x] User confirmed the replacement works after being asked to verify left-click, right-click, and copying.
- [x] Removed the old `tailswitch-dev` Ubuntu 24.04 container with `distrobox rm --force tailswitch-dev` after confirmation. Verified it no longer exists and `tailswitch-kde-dev` remains running.

The shared project directory, new executable, unrelated containers, and container images were not removed. The old ignored `build/dev` artifacts remain; use `build/kde-dev` for all current builds and launches.

## Next development step

- [ ] Investigate read-only Tailscale status/preferences and implement a tested CLI adapter for connection status and device listing.
- [ ] Continue the remaining compatibility and release checks in [COMPATIBILITY.md](COMPATIBILITY.md).

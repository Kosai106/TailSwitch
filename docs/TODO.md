# Follow-ups

## Build-container migration

- [x] Validate the `tailswitch-kde-dev` Ubuntu 26.04 build and automated tests (3/3 CTest entries passed).
- [x] Validate startup against SteamOS's host libraries and inspect native D-Bus menu export. Required `-fPIC` to avoid protected Qt data-symbol copy relocations; host smoke and protocol checks passed.
- [ ] Get user confirmation that left-click and right-click both open Plasma's tray menu, without the Wayland grabbing-popup warning, and that copying still works.
- [ ] Once the replacement is confirmed working, remove the old `tailswitch-dev` Ubuntu 24.04 container. Keep it until that decision; do not delete it during investigation.

If the replacement does not work, resolve the build-environment choice before removing the fallback. Container removal must not delete the shared project directory or unrelated containers/images. The old `build/dev` directory is separate from the new `build/kde-dev` directory; retiring old build artifacts can be considered after successful migration.

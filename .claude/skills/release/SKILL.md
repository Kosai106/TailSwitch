---
name: release
description: Cut a TailSwitch release end to end - bump the version, update the changelog, build and test the archive, verify on the Deck, merge, tag, and publish the GitHub Release. Use when asked to release, publish, tag, or ship a version.
---

# Releasing TailSwitch

A release is a `vX.Y.Z` tag on `main`. Pushing the tag runs `.github/workflows/ci.yml`, which rebuilds the archive in an `ubuntu:26.04` container, checks that the tag matches the CMake version, and attaches `tailswitch-X.Y.Z-linux-x86_64.tar.gz` plus its `.sha256` to a GitHub Release. Nothing is published by hand.

Work through the phases in order. Do not tag until every earlier phase is done.

## 1. Decide the version

- Read `CHANGELOG.md` and `git log <last tag>..main` to see what shipped since the last release.
- Pick the version with SemVer: bug fixes only bump the patch, new user-visible features bump the minor, and breaking changes to install paths, command-line flags, or behaviour bump the major. Anything below 1.0 may bump the minor for breaking changes as long as the changelog says so.
- Confirm with the user if the choice is not obvious.

## 2. Prepare the release branch

Create a branch such as `release/X.Y.Z` from an up-to-date `main`. Commit messages stay plain: no Co-Authored-By or session attribution lines.

1. Set the version in `CMakeLists.txt`: `project(TailSwitch VERSION X.Y.Z LANGUAGES CXX)`. That single value feeds `--version`, the About dialog, startup diagnostics, and the archive name. Nothing else hard-codes it.
2. Move the changelog's pending entries under a `## [X.Y.Z] - YYYY-MM-DD` heading, following Keep a Changelog sections (Added, Changed, Fixed, Removed). Add the link reference at the bottom: `[X.Y.Z]: https://github.com/Kosai106/TailSwitch/releases/tag/vX.Y.Z`.
3. If runtime requirements changed (Qt, KF6, glibc, SteamOS release), update the README's Requirements section and the table in `docs/COMPATIBILITY.md`. Measure them from the release binary with `objdump -T build/release/tailswitch | grep -oE 'GLIBC_[0-9.]+|Qt_[0-9.]+' | sort -Vu`.
4. Tick off or rewrite the checklist in `docs/TODO.md` so it describes this release.

## 3. Build and test the archive locally

Run from the host:

```sh
distrobox enter tailswitch-kde-dev -- ./packaging/make-release.sh
```

It must end with `100% tests passed` and print the archive path under `dist/`. If tests fail, fix them on the branch; never release from a red build. Then confirm the version stamp:

```sh
./build/release/tailswitch --version
tar tzf dist/tailswitch-X.Y.Z-linux-x86_64.tar.gz
```

The archive must contain `tailswitch`, `tailswitch.desktop`, `tailswitch.svg`, `install.sh`, `uninstall.sh`, `LICENSE`, `README.md`, `CHANGELOG.md`, and `TRADEMARKS.md`.

## 4. Verify on the Deck

Automated tests never touch the real tray, clipboard, or autostart, so these need a person at the Deck. Ask the user to do them and wait for the answers; do not tick them yourself.

1. Extract the archive somewhere outside the repo, run `./install.sh`, and launch TailSwitch from the application menu. The launcher icon and tray icon must render.
2. Left-click and right-click both open the menu; copying a device address works.
3. Turn on **Start at login** from the installed copy, log out and back into Desktop Mode, and confirm exactly one tray icon appears. Turn it off again if the user does not want it.
4. Run `./uninstall.sh` if the user does not want to keep the installed copy.

A dry run of the installer without touching the real home is `HOME=/some/scratch/dir ./install.sh`.

## 5. Merge

Push the branch, open a pull request, and wait for the CI run to pass. The CI build must be green before merging, because the tag build uses the same workflow. Merge the PR into `main`.

## 6. Tag and publish

On an up-to-date `main`:

```sh
git switch main
git pull --ff-only
git tag -a vX.Y.Z -m "TailSwitch X.Y.Z"
git push origin vX.Y.Z
```

Tagging and pushing are the publishing step. Confirm with the user before pushing the tag, then watch the Actions run for the tag. When it finishes, the release exists at `https://github.com/Kosai106/TailSwitch/releases/tag/vX.Y.Z` with the archive and checksum attached.

## 7. Finish the release page

The workflow auto-generates notes from merged pull requests, which is not enough for users.

- Paste the changelog section for this version at the top of the release notes.
- State the runtime requirement in the notes (currently SteamOS 3.9 or newer because the binary needs Qt 6.10), and the one-line install instructions from the README.
- Mark the release as a pre-release if the user considers it early.

## Troubleshooting

- **"Tag vX.Y.Z does not match CMake version"**: the CMake version was not bumped or the tag has a typo. Delete the tag locally and remotely (`git tag -d vX.Y.Z && git push origin :refs/tags/vX.Y.Z`), fix `main`, and tag again.
- **Release step fails with 403**: check Settings > Actions > General > Workflow permissions, and that the third-party `softprops/action-gh-release` action is allowed.
- **desktop_behaviors fails only in CI**: the container is missing a runtime plugin. The Qt SVG icon engine (`qt6-svg-plugins`) was the first such case; compare the apt list in the workflow with `docs/DEVELOPMENT.md`.
- **Binary will not load on a Deck**: it was built against a newer Qt than SteamOS ships. `install.sh` reports this. Rebuild in a container whose Qt is no newer than the target SteamOS release.

## Never do these

- Never tag a commit that is not on `main`, or whose CI run is not green.
- Never edit a published tag. Ship a new patch version instead.
- Never commit real tailnet data, device names, or `tailscale status` output while preparing fixtures or notes.
- Never run mutating Tailscale commands during verification. Release checks are read-only.

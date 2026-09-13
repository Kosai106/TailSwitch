#!/usr/bin/env bash
# Install TailSwitch into the current user's home directory.
# Nothing outside $HOME is touched, so SteamOS's read-only system stays intact.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
data_home="${XDG_DATA_HOME:-$HOME/.local/share}"
bin_dir="$HOME/.local/bin"
app_dir="$data_home/applications"
icon_dir="$data_home/icons/hicolor/scalable/apps"

for file in tailswitch tailswitch.desktop tailswitch.svg; do
    if [ ! -f "$here/$file" ]; then
        echo "Missing $file next to this script; use the release archive as extracted." >&2
        exit 1
    fi
done

if ! command -v tailscale >/dev/null 2>&1 && [ ! -x /opt/tailscale/tailscale ]; then
    echo "Note: the Tailscale CLI was not found. Install and log in to Tailscale first;" >&2
    echo "TailSwitch only displays the status of an existing installation." >&2
fi

# Fail early with a readable message if this build cannot load against the
# system's Qt/KDE libraries (for example an older SteamOS release).
if ! QT_QPA_PLATFORM=offscreen "$here/tailswitch" --version >/dev/null 2>"$here/.loader-check"; then
    echo "This TailSwitch build cannot run on this system:" >&2
    sed 's/^/  /' "$here/.loader-check" >&2
    rm -f "$here/.loader-check"
    echo "It needs the Qt 6 and KDE Frameworks 6 libraries shipped with SteamOS 3.9 or newer." >&2
    echo "On older releases, build from source instead (see docs/DEVELOPMENT.md)." >&2
    exit 1
fi
rm -f "$here/.loader-check"

mkdir -p "$bin_dir" "$app_dir" "$icon_dir"
install -m 755 "$here/tailswitch" "$bin_dir/tailswitch"
install -m 644 "$here/tailswitch.svg" "$icon_dir/tailswitch.svg"
# The launcher's PATH may not include ~/.local/bin, so point at the binary directly.
sed -e "s|^Exec=.*|Exec=\"$bin_dir/tailswitch\"|" \
    -e "s|^TryExec=.*|TryExec=$bin_dir/tailswitch|" \
    "$here/tailswitch.desktop" > "$app_dir/tailswitch.desktop"
chmod 644 "$app_dir/tailswitch.desktop"

command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$app_dir" >/dev/null 2>&1 || true
command -v kbuildsycoca6 >/dev/null 2>&1 && kbuildsycoca6 >/dev/null 2>&1 || true

cat <<MSG
Installed TailSwitch:
  $bin_dir/tailswitch
  $app_dir/tailswitch.desktop
  $icon_dir/tailswitch.svg

Launch it from the application menu (Network > TailSwitch) or run:
  $bin_dir/tailswitch
Turn on "Start at login" from the tray menu if you want it at every Desktop Mode login.
Uninstall with: $here/uninstall.sh
MSG

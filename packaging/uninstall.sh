#!/usr/bin/env bash
# Remove TailSwitch from the current user's home directory.
# Tailscale itself, its login, and its settings are left untouched.
set -euo pipefail

data_home="${XDG_DATA_HOME:-$HOME/.local/share}"
config_home="${XDG_CONFIG_HOME:-$HOME/.config}"
files=(
    "$HOME/.local/bin/tailswitch"
    "$data_home/applications/tailswitch.desktop"
    "$data_home/icons/hicolor/scalable/apps/tailswitch.svg"
    "$config_home/autostart/tailswitch.desktop"
)

if pgrep -x tailswitch >/dev/null 2>&1; then
    echo "Stopping the running TailSwitch tray (this does not disconnect Tailscale)."
    pkill -x tailswitch || true
fi

for file in "${files[@]}"; do
    if [ -e "$file" ]; then
        rm -f "$file"
        echo "Removed $file"
    fi
done

command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$data_home/applications" >/dev/null 2>&1 || true
command -v kbuildsycoca6 >/dev/null 2>&1 && kbuildsycoca6 >/dev/null 2>&1 || true
echo "TailSwitch has been uninstalled. Tailscale is unchanged."

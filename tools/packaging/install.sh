#!/bin/sh
# User-local installation: stage and validate before replacing a managed install.
set -eu
source_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)
prefix=${1:-"$HOME/.local/opt/xdb"}
bindir=${2:-"$HOME/.local/bin"}
case "$prefix" in /*) ;; *) echo 'Installation prefix must be absolute' >&2; exit 2 ;; esac
case "$bindir" in /*) ;; *) echo 'Command directory must be absolute' >&2; exit 2 ;; esac
[ -x "$source_dir/bin/xdb" ] || { echo 'Run install.sh from an extracted xdb runtime package' >&2; exit 1; }
[ ! -L "$prefix" ] || { echo 'Installation prefix must not be a symlink' >&2; exit 1; }
mkdir -p "$(dirname -- "$prefix")" "$bindir"
prefix_parent=$(CDPATH='' cd -- "$(dirname -- "$prefix")" && pwd -P)
prefix="$prefix_parent/$(basename -- "$prefix")"
bindir=$(CDPATH='' cd -- "$bindir" && pwd -P)
case "$prefix/" in "$source_dir/"|"$source_dir/"*) echo 'Install outside the extracted package' >&2; exit 1 ;; esac
case "$source_dir/" in "$prefix/"*) echo 'Source package must not be inside the installation' >&2; exit 1 ;; esac
case "$bindir/" in "$prefix/"*) echo 'Command directory must be outside the installation' >&2; exit 1 ;; esac
if [ -e "$prefix" ] && [ ! -d "$prefix" ]; then echo 'Installation prefix is not a directory' >&2; exit 1; fi
if [ -d "$prefix" ] && [ -n "$(ls -A "$prefix")" ] && [ ! -f "$prefix/.xdb-install" ]; then
    echo "Refusing to overwrite a directory not owned by this installer: $prefix" >&2; exit 1
fi
if [ -e "$bindir/xdb" ] || [ -L "$bindir/xdb" ]; then
    if [ ! -L "$bindir/xdb" ] || [ "$(readlink "$bindir/xdb")" != "$prefix/bin/xdb" ]; then
        echo "Existing command left untouched: $bindir/xdb" >&2; exit 1
    fi
fi
lock="$prefix.install-lock"
mkdir "$lock" 2>/dev/null || { echo 'Another install is running (or its install-lock needs inspection)' >&2; exit 1; }
work=$(mktemp -d "$prefix_parent/.xdb-install.XXXXXX") || { rmdir "$lock"; exit 1; }
backup="$work/previous"
stage="$work/new"
switched=false
finished=false
cleanup() {
    status=$?
    trap - EXIT HUP INT TERM
    if [ "$finished" = false ] && [ "$switched" = true ]; then
        rm -rf "$prefix"
        if [ -d "$backup" ]; then mv "$backup" "$prefix"; fi
    fi
    rm -rf "$work"
    rmdir "$lock"
    exit "$status"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
mkdir "$stage"
for item in bin lib share INSTALL.md INSTALL_zh.md ANDROID.md ANDROID_zh.md install.sh; do
    [ ! -e "$source_dir/$item" ] || cp -R "$source_dir/$item" "$stage/"
done
: > "$stage/.xdb-install"
# This must run without contacting a phone or starting a background service.
"$stage/bin/xdb" --help >/dev/null
if [ -x "$prefix/bin/xdb" ]; then
    # Protocol-incompatible/non-xdb listeners are never killed by this command.
    "$prefix/bin/xdb" kill-server || {
        echo 'Could not stop the existing service; installation unchanged. Stop it using its original xdb first.' >&2
        exit 1
    }
fi
if [ -d "$prefix" ]; then mv "$prefix" "$backup"; fi
switched=true
mv "$stage" "$prefix"
ln -sfn "$prefix/bin/xdb" "$bindir/xdb"
finished=true
printf 'Installed: %s\nCommand: %s/xdb\n' "$prefix" "$bindir"
printf 'Server will start on the next client connection; pairing records and DDI cache are retained.\n'
case ":$PATH:" in *":$bindir:"*) ;; *) printf 'Add this directory to PATH: %s\n' "$bindir" ;; esac

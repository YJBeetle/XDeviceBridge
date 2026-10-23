#!/bin/sh
# User-local installation: no sudo, no shell-profile edits.
set -eu
source_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd -P)
prefix=${1:-"$HOME/.local/opt/xdb"}
bindir=${2:-"$HOME/.local/bin"}
case "$prefix" in /*) ;; *) echo 'Installation prefix must be absolute' >&2; exit 2 ;; esac
[ -x "$source_dir/bin/xdb" ] || { echo 'Run install.sh from an extracted xdb runtime package' >&2; exit 1; }
[ "$prefix" != "$source_dir" ] || { echo 'Choose an installation directory different from this package' >&2; exit 1; }
if [ -d "$prefix" ] && [ -n "$(ls -A "$prefix")" ] && [ ! -f "$prefix/.xdb-install" ]; then
    echo "Refusing to overwrite a directory not owned by this installer: $prefix" >&2
    exit 1
fi
mkdir -p "$prefix" "$bindir"
for item in bin lib share INSTALL.md install.sh; do
    [ ! -e "$source_dir/$item" ] || cp -R "$source_dir/$item" "$prefix/"
done
: > "$prefix/.xdb-install"
if [ -e "$bindir/xdb" ] && [ ! -L "$bindir/xdb" ]; then
    echo "Existing non-symlink command left untouched: $bindir/xdb" >&2
    exit 1
fi
ln -sfn "$prefix/bin/xdb" "$bindir/xdb"
printf 'Installed: %s\nCommand: %s/xdb\n' "$prefix" "$bindir"
case ":$PATH:" in *":$bindir:"*) ;; *) printf 'Add this directory to PATH: %s\n' "$bindir" ;; esac

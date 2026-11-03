#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Paolo Sereno <pablitos1024@gmail.com>
#
# SPDX-License-Identifier: MIT

# Environment without any font, for the tests that must hold when no font is installed (fontconfig only: Linux).
#
#   source tools/nofont-env.sh [DIR]
#
# Writes DIR/fonts.conf (default: a new temporary directory), a fontconfig configuration whose only font directory
# is the empty DIR/empty, and exports FONTCONFIG_FILE for the commands that follow in the same shell. It fails
# (returns 1, FONTCONFIG_FILE left as it was) if fc-list is missing or still lists a font: then the tests would run
# with fonts while claiming to run without.
#
# Example:
#   (source tools/nofont-env.sh && QT_QPA_PLATFORM=offscreen ./build/bin/tst_TankDisplay)

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    echo "nofont-env.sh: source it, so that FONTCONFIG_FILE reaches the commands that follow:" >&2
    echo "  source tools/nofont-env.sh [DIR]" >&2
    exit 2
fi

qiw_nofont_setup() {
    local dir="${1:-}"
    if [[ -z "$dir" ]]; then
        dir="$(mktemp -d "${TMPDIR:-/tmp}/qiw-nofonts.XXXXXX")" || return 1
    fi
    mkdir -p "$dir/empty" "$dir/cache" || return 1
    dir="$(cd "$dir" && pwd)" || return 1
    # Pure bash, in a subshell so that the options do not reach the caller's shell
    if ! (shopt -s nullglob dotglob; entries=("$dir/empty"/*); ((${#entries[@]} == 0))); then
        echo "nofont-env.sh: $dir/empty is not empty" >&2
        return 1
    fi
    cat > "$dir/fonts.conf" <<EOF
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<!-- Written by tools/nofont-env.sh: no font directory but an empty one -->
<fontconfig>
  <dir>$dir/empty</dir>
  <cachedir>$dir/cache</cachedir>
</fontconfig>
EOF
    if ! command -v fc-list > /dev/null 2>&1; then
        echo "nofont-env.sh: fc-list not found: cannot check that no font is visible" >&2
        return 1
    fi
    local fonts
    fonts="$(FONTCONFIG_FILE="$dir/fonts.conf" fc-list 2>&1)"
    if [[ -n "$fonts" ]]; then
        echo "nofont-env.sh: fc-list still sees fonts (or fails) with $dir/fonts.conf:" >&2
        printf '%s\n' "$fonts" | head -5 >&2
        return 1
    fi
    export FONTCONFIG_FILE="$dir/fonts.conf"
    echo "nofont-env.sh: FONTCONFIG_FILE=$FONTCONFIG_FILE (fc-list: no font)" >&2
}

qiw_nofont_setup "$@"
qiw_nofont_status=$?
unset -f qiw_nofont_setup
if [[ $qiw_nofont_status -ne 0 ]]; then
    unset qiw_nofont_status
    return 1
fi
unset qiw_nofont_status

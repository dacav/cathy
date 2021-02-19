#!/bin/sh

set -e

atexit() {
    [ -z "$tmpdir" ] || rm -rf "$tmpdir"
}
trap atexit EXIT

tmpdir="$(mktemp -d)"

filehier="$tmpdir/hier"
mkdir "$filehier"

listout() {
    [ "$1" ] || return
    printf "%s\0" "$@"
}

mkfile() {
    [ "$1" ] || return
    printf "%s\n" "$1" > "$filehier/$1"
    listout "$filehier/$1"
}

duplicate() {
    [ "$1" ] || return
    cp "$filehier/$1" "$filehier/$1.duplicate"
    listout "$filehier/$1.duplicate"
}

change_mtime() {
    sleep 1
    touch "$filehier/$1"
}

hardlink() (
    [ "$1" ] || return
    cd "$filehier"
    ln "$1" "$1.hardlink"
    listout "$filehier/$1.hardlink"
)

softlink() (
    [ "$1" ] || return
    cd "$filehier"
    ln -s "$1" "$1.softlink"
    listout "$filehier/$1.softlink"
)

ok() {
    local result="fail"

    if "$@" 2>&3; then
        result="ok"
    fi

    printf >&2 "%s - %s\n" $result "$*"
}

build_filetree() {
    mkfile foo.jpeg
    hardlink foo.jpeg
    softlink foo.jpeg
    mkfile bar.mpv
    duplicate bar.mpv
    duplicate foo.jpeg
    change_mtime foo.jpeg
}

echo >&2 "NOTE: logging in test.log"
exec 3>test.log

cd $tmpdir
ok build_filetree > input
ok cathy < ./input

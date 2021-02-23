#!/bin/sh

set -e

echo >&2 "# NOTE: logging in test.log"
exec 3>test.log
failures=
next_test_id=

tree() {
	if command -v tree; then
		command tree "$@"
	else
		ls -lR "$@"
	fi
}

atexit() {
	if [ "$failures" ]; then
		echo >&2 "registered $failures failures"
	else
		echo >&2 "SUCCESS!"
	fi

	printf >&3 "== EXIT ==\n"

	if [ "$tmpdir" ]; then
		rm -rf "$tmpdir"
	fi

	exit ${failures:-0}
}
trap atexit EXIT

reset() {
	[ -z "$tmpdir" ] || rm -rf "$tmpdir"

	tmpdir="$(mktemp -d)"
	filehier="$tmpdir/hier"
	mkdir "$filehier"

	# The file descriptor 4 is later used to feed cathy with the
	# equivalent of a find hier -print0.
	exec 4>"$tmpdir/input"
}

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
	cp -a "$filehier/$1" "$filehier/$1.duplicate"
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

exists() {
	test -e "$filehier/$1"
}

is_hashed() {
	set -x
	local file

	file="$filehier/${1:?}"
	find "$tmpdir/by-hash" -type l |
		while read -r link; do
			link="$(realpath -e "$link")" || return
			[ "$file" = "$link" ] && {
				echo 1
				break
			}
		done |
		grep -q 1
}

diag() {
	if [ "$1" ]; then
		printf %s\\n "$*" | sed 's/^/# /'
	else
		sed 's/^/# /'
	fi
} >&2

run() {
	if [ "$next_test_id" ]; then
		diag "------ END TEST $next_test_id ------"
	fi
	reset
	next_test_id=$((next_test_id + 1))
	diag "------ TEST $next_test_id: $* ------"

    "$@"
}

ok() {
	local result="fail"

	printf >&3 "\n== ok: %s ==\n" "$*"
	if ( set -x; "$@" ) 2>&3; then
		result="ok"
	else
		failures=$((failures + 1))
	fi

	printf >&2 "%s - %s\n" $result "$*"
}

fail() {
	local result="fail"

	printf >&3 "\n== fail: %s ==\n" "$*"
	if ( set -x; "$@" ) 2>&3; then
		failures=$((failures + 1))
	else
		result="ok"
	fi

	printf >&2 "%s - ! %s\n" $result "$*"
}

cathy() (
	cd "$tmpdir"
	command cathy "$@" <./input
)

test_links() {
	ok mkfile foo.jpeg >&4
	ok hardlink foo.jpeg >&4
	ok softlink foo.jpeg >&4
	ok cathy -r

	diag <<-END
	All files exists, since they share the same inode, so no space
	is claimed by their removal.
	END
	ok exists foo.jpeg
	ok exists foo.jpeg.hardlink
	ok exists foo.jpeg.softlink

	diag <<-END
	Only one of the files is hashed, that is the first one listed to
	cathy.
	END
	ok is_hashed foo.jpeg
	fail is_hashed foo.jpeg.hardlink
	fail is_hashed foo.jpeg.softlink
}

test_duplicates() {
	diag <<-END
	ok mkfile foo.jpeg >&4
	We duplicate a file and verify that cathy removes it in favour of the
	original copy, which is listed first to cathy's stdin.
	END
	ok mkfile foo.jpeg >&4
	ok duplicate foo.jpeg >&4
	ok exists foo.jpeg.duplicate
	ok cathy -r
	fail exists foo.jpeg.duplicate

	diag <<-END
	As in the previous test, only the original (first listed) file is hashed,
	and nothing else (especially the duplicate, which no longer exists).
	END
	ok is_hashed foo.jpeg
	fail is_hashed foo.jpeg.hardlink
	fail is_hashed foo.jpeg.softlink
	fail is_hashed foo.jpeg.duplicate
}

test_always_keep_the_oldest() {
	diag <<-END
	We duplicate a file and then move forward the timestamp of the original
	one.  Now, even if the original file is listed to cathy's stdin before the
	copy, the original is removed, since it is newer than the copy, according
	to its modification time (mtime).
	END
	ok mkfile foo.jpeg >&4
	ok duplicate foo.jpeg >&4
	ok exists foo.jpeg.duplicate
	ok change_mtime foo.jpeg
	ok cathy -r

	diag<<-END
	The foo.jpeg.duplicate file is hashed, while the original foo.jpeg
	(which is removed, by previous test) is of course not hashed.
	END
	fail exists foo.jpeg
	ok is_hashed foo.jpeg.duplicate
	fail is_hashed foo.jpeg
}

run test_links
run test_duplicates
run test_always_keep_the_oldest

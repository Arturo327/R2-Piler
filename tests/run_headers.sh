#!/bin/sh

# usage: tests/run_headers.sh <r2p> <dir>
#
# Each case is a directory with a test.conf:
#   source <file>          source to compile (default a.r2)
#   dir <path>             header directory passed to --header (default include)
#   output <path>          expected header path inside the workspace
#                          (default <dir>/<source-stem>.h)
#   expected <file>        expected header content, case-relative (default expected.h)
#   args <text>            extra compiler arguments
#   status <n>             expected compiler exit status (default 0)
#   stderr <file>          expected compiler stderr, case-relative
#   no_header              do not pass --header
#   no_output              assert that no header is written
#   c_source <file>        host C file to compile, link and run
#   cpp_source <file>      host C++ file to compile, link and run
#   clang                  also compile the C host with clang
#   run_exit <n>           expected exit code of the host programs (default 0)
#
# `$ROOT` in expected files expands to the case workspace.

bin="$1"; dir="$2"
[ -x "$bin" ] || { echo "usage: $0 <path-to-r2p> [fixture-dir]"; exit 1; }
[ -d "$dir" ] || { echo "No tests found in $dir"; exit 1; }
bin=$(cd "$(dirname "$bin")" && pwd)/$(basename "$bin")

root=$(mktemp -d)
trap 'rm -rf "$root"' EXIT
passed=0; failed=0

pass() { echo "PASS $1"; passed=$((passed+1)); }
fail() { echo "FAIL $1"; failed=$((failed+1)); }
show() { [ -s "$1" ] && sed 's/^/  /' "$1"; return 0; }

check_err() { # expected-file actual-file label
	if [ -n "$1" ]; then
		sed "s|\$ROOT|$ws|g" "$1" >"$root/expected"
		diff -u "$root/expected" "$2" || { echo "  $3 differs"; return 1; }
	elif [ -s "$2" ]; then
		echo "  unexpected $3:"
		show "$2"
		return 1
	fi
	return 0
}

run_host() { # compiler source output
	"$1" -I"$ws" "$ws/$2" "$ws/out.s" -o "$ws/$3" 2>"$root/host.err" || {
		echo "  cannot build $2 with $1"; show "$root/host.err"; return 1
	}
	"$ws/$3" 2>"$root/host.err"
	status=$?
	[ "$status" -eq "$want_exit" ] || {
		echo "  $2 exited $status, want $want_exit"; show "$root/host.err"; return 1
	}
	return 0
}

for conf in "$dir"/*/test.conf; do
	[ -e "$conf" ] || { echo "No tests found in $dir"; exit 1; }
	case_dir=${conf%/test.conf}
	name=${case_dir##*/}
	ws="$root/$name"
	mkdir -p "$ws" || { fail "$name"; continue; }
	cp "$case_dir"/*.r2 "$ws"/ 2>/dev/null
	cp "$case_dir"/*.r2i "$ws"/ 2>/dev/null
	cp "$case_dir"/*.c "$case_dir"/*.cpp "$ws"/ 2>/dev/null
	cp -R "$case_dir/include" "$ws/" 2>/dev/null

	src=a.r2; hdir=include; out=""; expected=expected.h
	want=0; want_err=""; no_header=0; no_output=0
	c=""; cpp=""; use_clang=0; want_exit=0; extra=""
	ok=1

	while read -r key val || [ -n "$key" ]; do
		case "$key" in
		''|'#'*) continue;;
		source) src=$val;;
		dir) hdir=$val;;
		output) out=$val;;
		expected) expected=$val;;
		args) extra="$extra $val";;
		status) want=$val;;
		stderr) want_err="$case_dir/$val";;
		no_header) no_header=1;;
		no_output) no_output=1;;
		c_source) c=$val;;
		cpp_source) cpp=$val;;
		clang) use_clang=1;;
		run_exit) want_exit=$val;;
		*) echo "  unknown directive '$key'"; ok=0;;
		esac
	done <"$conf"

	[ -n "$out" ] || out="$hdir/${src%.r2}.h"

	if [ "$ok" -eq 1 ]; then
		hflag=""
		[ "$no_header" -eq 1 ] || hflag="--header=$hdir"
		(cd "$ws" && "$bin" "$src" -o out.s -m modules $hflag $extra \
			>"$root/stdout" 2>"$root/stderr")
		status=$?
		[ "$status" -eq "$want" ] || {
			echo "  status $status, want $want"; show "$root/stderr"; ok=0
		}
		check_err "$want_err" "$root/stderr" "stderr" || ok=0
	fi

	if [ "$ok" -eq 1 ] && [ "$want" -eq 0 ]; then
		if [ "$no_output" -eq 1 ]; then
			found=$(find "$ws/$hdir" -name '*.h' -print 2>/dev/null | head -n 1)
			[ -z "$found" ] || { echo "  unexpected $found"; ok=0; }
		else
			[ -f "$ws/$out" ] || { echo "  missing $out"; ok=0; }
			if [ "$ok" -eq 1 ] && [ -f "$case_dir/$expected" ]; then
				check_err "$case_dir/$expected" "$ws/$out" "header" || ok=0
			fi
		fi
	fi

	if [ "$ok" -eq 1 ] && [ "$want" -eq 0 ]; then
		if [ "$ok" -eq 1 ] && [ -n "$c" ]; then
			run_host gcc "$c" host_c || ok=0
		fi
		if [ "$ok" -eq 1 ] && [ "$use_clang" -eq 1 ] && [ -n "$c" ]; then
			run_host clang "$c" host_clang || ok=0
		fi
		if [ "$ok" -eq 1 ] && [ -n "$cpp" ]; then
			run_host g++ "$cpp" host_cpp || ok=0
		fi
	fi

	if [ "$ok" -eq 1 ]; then pass "$name"; else fail "$name"; fi
done

echo "$passed passed, $failed failed"
[ "$failed" -eq 0 ]

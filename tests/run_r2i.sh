#!/bin/sh

# usage: tests/run_r2i.sh <r2p> <dir>
#
# Each case is a directory with a test.conf:
#   source <file>          source to compile (default a.r2)
#   output <path>          expected .r2i path inside the modules dir (default a.r2i)
#   dir <path>             modules directory passed to -m (default modules)
#   args <text>            extra compiler arguments
#   status <n>             expected compiler exit status (default 0)
#   stderr <file>          expected stderr, case-relative (default: must be empty)
#   *.r2i files            copied to the workspace as prebuilt imported interfaces
#   no_output              assert that no .r2i file is written
#   second <file>          second source compiled afterwards in the same workspace
#   second_status <n>      expected exit status of the second compile (default 0)
#   second_stderr <file>   expected stderr of the second compile
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

check_file() { # expected-file actual-file label
	[ -f "$2" ] || { echo "  missing $2"; return 1; }
	sed "s|\$ROOT|$ws|g" "$1" >"$root/expected"
	diff -u "$root/expected" "$2" || { echo "  $3 differs"; return 1; }
	return 0
}

for conf in "$dir"/*/test.conf; do
	[ -e "$conf" ] || { echo "No tests found in $dir"; exit 1; }
	case_dir=${conf%/test.conf}
	name=${case_dir##*/}
	ws="$root/$name"
	mkdir -p "$ws"
	cp "$case_dir"/*.r2 "$ws"/ 2>/dev/null
	cp "$case_dir"/*.r2i "$ws"/ 2>/dev/null

	src=a.r2; out=a.r2i; mdir=modules
	want=0; want_err=""; no_output=0; extra=""
	second=""; second_want=0; second_err=""
	ok=1

	while IFS=' ' read -r key val || [ -n "$key" ]; do
		case "$key" in
		''|'#'*) continue;;
		source) src=$val;;
		output) out=$val;;
		dir) mdir=$val;;
		args) extra="$extra $val";;
		status) want=$val;;
		stderr) want_err="$case_dir/$val";;
		no_output) no_output=1;;
		second) second=$val;;
		second_status) second_want=$val;;
		second_stderr) second_err="$case_dir/$val";;
		*) echo "  unknown directive '$key'"; ok=0;;
		esac
	done <"$conf"

	if [ "$ok" -eq 1 ]; then
		(cd "$ws" && "$bin" "$src" -o out.s -m "$mdir" $extra >"$root/stdout" 2>"$root/stderr")
		status=$?
		[ "$status" -eq "$want" ] || {
			echo "  status $status, want $want"; show "$root/stderr"; ok=0
		}
		check_err "$want_err" "$root/stderr" stderr || ok=0
	fi

	if [ "$ok" -eq 1 ] && [ "$no_output" -eq 1 ]; then
		found=$(find "$ws" -name '*.r2i' -print 2>/dev/null | head -n 1)

		[ -z "$found" ] || { echo "  unexpected $found"; ok=0; }
	elif [ "$ok" -eq 1 ]; then
		check_file "$case_dir/expected.r2i" "$ws/$mdir/$out" output || ok=0
	fi

	if [ "$ok" -eq 1 ] && [ -n "$second" ]; then
		(cd "$ws" && "$bin" "$second" -o out2.s -m "$mdir" $extra >"$root/stdout2" 2>"$root/stderr2")
		status=$?
		[ "$status" -eq "$second_want" ] || {
			echo "  second status $status, want $second_want"
			show "$root/stderr2"
			ok=0
		}
		check_err "$second_err" "$root/stderr2" "second stderr" || ok=0
	fi

	if [ "$ok" -eq 1 ]; then pass "$name"; else fail "$name"; fi
done

echo "$passed passed, $failed failed"
[ "$failed" -eq 0 ]
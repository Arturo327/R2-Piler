#!/bin/sh
# usage: tests/run_suite.sh <r2p> <flag> <dir>
bin="$1"; flag="$2"; dir="$3"
passed=0; failed=0
tmp_out=$(mktemp); tmp_err=$(mktemp)
trap 'rm -f "$tmp_out" "$tmp_err"' EXIT

for src in "$dir"/*_src.r2; do
	[ -e "$src" ] || { echo "No tests found in $dir"; exit 1; }
	base=${src%_src.r2}
	want_status=0
	[ -f "${base}_status.txt" ] && want_status=$(cat "${base}_status.txt")
	"$bin" "$flag" "$src" >"$tmp_out" 2>"$tmp_err"
	status=$?
	ok=1
	if [ ! -f "${base}_result.txt" ]; then
		echo "FAIL $src: missing ${base}_result.txt"; ok=0
	elif [ "$status" -ne "$want_status" ]; then
		echo "FAIL $src: status $status, want $want_status"; ok=0
	elif ! diff -u "${base}_result.txt" "$tmp_out"; then
		echo "FAIL $src: stdout differs"; ok=0
	elif [ -f "${base}_stderr.txt" ]; then
		diff -u "${base}_stderr.txt" "$tmp_err" || { echo "FAIL $src: stderr differs"; ok=0; }
	elif [ -s "$tmp_err" ]; then
		echo "FAIL $src: unexpected stderr:"; cat "$tmp_err"; ok=0
	fi
	if [ "$ok" -eq 1 ]; then echo "PASS $src"; passed=$((passed+1)); else failed=$((failed+1)); fi
done
echo "$passed passed, $failed failed"
[ "$failed" -eq 0 ]

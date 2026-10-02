#!/bin/sh
# usage: tests/run_asm.sh <r2p> <dir>
# Assembler suite: for each <name>_src.r2, compile to assembly with r2p,
# assemble with gcc, run the binary and check its exit code.
# Expected exit code lives in <name>_exit.txt (default 0).
# Expected compiler stderr lives in <name>_stderr.txt (default: must be empty).
bin="$1"; dir="$2"
passed=0; failed=0
tmp_out=$(mktemp); tmp_err=$(mktemp); tmp_gcc=$(mktemp)
tmp_asm=$(mktemp --suffix=.s); tmp_exe=$(mktemp)
trap 'rm -f "$tmp_out" "$tmp_err" "$tmp_gcc" "$tmp_asm" "$tmp_exe"' EXIT

for src in "$dir"/*_src.r2; do
	[ -e "$src" ] || { echo "No tests found in $dir"; exit 1; }
	base=${src%_src.r2}
	want_exit=0
	[ -f "${base}_exit.txt" ] && want_exit=$(cat "${base}_exit.txt")
	ok=1
	"$bin" "$src" -o "$tmp_asm" >"$tmp_out" 2>"$tmp_err"
	cstat=$?
	if [ "$cstat" -ne 0 ]; then
		echo "FAIL $src: r2p exited with $cstat"; cat "$tmp_err"; ok=0
	elif [ -s "$tmp_out" ]; then
		echo "FAIL $src: unexpected stdout on compile:"; cat "$tmp_out"; ok=0
	elif [ -f "${base}_stderr.txt" ]; then
		diff -u "${base}_stderr.txt" "$tmp_err" || { echo "FAIL $src: stderr differs"; ok=0; }
	elif [ -s "$tmp_err" ]; then
		echo "FAIL $src: unexpected stderr:"; cat "$tmp_err"; ok=0
	fi
	if [ "$ok" -eq 1 ]; then
		gcc "$tmp_asm" -o "$tmp_exe" 2>"$tmp_gcc"
		if [ $? -ne 0 ]; then
			echo "FAIL $src: gcc failed:"; cat "$tmp_gcc"; ok=0
		else
			"$tmp_exe"
			code=$?
			if [ "$code" -ne "$want_exit" ]; then
				echo "FAIL $src: exit $code, want $want_exit"; ok=0
			fi
		fi
	fi
	if [ "$ok" -eq 1 ]; then echo "PASS $src"; passed=$((passed+1)); else failed=$((failed+1)); fi
done
echo "$passed passed, $failed failed"
[ "$failed" -eq 0 ]

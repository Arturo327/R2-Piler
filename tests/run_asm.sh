#!/bin/sh

# usage: tests/run_asm.sh <r2p> <dir>
# Assembler suite: for each <name>_src.r2, compile to assembly with r2p,
# assemble with gcc, run the binary and check its exit code.
# Expected exit code lives in <name>_exit.txt (default 0).
# Expected compiler stderr lives in <name>_stderr.txt (default: must be empty).

# Timing: one run of these tiny programs takes <1ms, below the noise floor
# of a single `date` call (~0.5ms fork+exec each). So after the correctness
# check, each test is compiled COMPILE_REPS times and executed RUN_REPS
# times; the reported numbers are per-run means (batch wall time / reps),
# which amortizes timer overhead and scheduler jitter. Override with e.g.
# RUN_REPS=50 COMPILE_REPS=10 tests/run_asm.sh <r2p> <dir> (gcc is untimed).

bin="$1"; dir="$2"
RUN_REPS=${RUN_REPS:-20}
COMPILE_REPS=${COMPILE_REPS:-5}
passed=0; failed=0
compile_total_ns=0; run_total_ns=0
tmp_out=$(mktemp); tmp_err=$(mktemp); tmp_gcc=$(mktemp)
tmp_asm=$(mktemp --suffix=.s); tmp_exe=$(mktemp)
trap 'rm -f "$tmp_out" "$tmp_err" "$tmp_gcc" "$tmp_asm" "$tmp_exe"' EXIT

fmt_ms() { printf "%d.%03dms" $(($1 / 1000000)) $((($1 % 1000000) / 1000)); }

for src in "$dir"/*_src.r2; do
	[ -e "$src" ] || { echo "No tests found in $dir"; exit 1; }
	base=${src%_src.r2}
	want_exit=0
	[ -f "${base}_exit.txt" ] && want_exit=$(cat "${base}_exit.txt")
	ok=1
	compile_ns=0; run_ns=0
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
		t0=$(date +%s%N)
		i=0
		while [ "$i" -lt "$COMPILE_REPS" ]; do
			"$bin" "$src" -o "$tmp_asm" >/dev/null 2>&1
			i=$((i + 1))
		done
		t1=$(date +%s%N)
		compile_ns=$(( (t1 - t0) / COMPILE_REPS ))
		compile_total_ns=$((compile_total_ns + compile_ns))
		gcc "$tmp_asm" -o "$tmp_exe" 2>"$tmp_gcc"
		if [ $? -ne 0 ]; then
			echo "FAIL $src: gcc failed:"; cat "$tmp_gcc"; ok=0
		else
			"$tmp_exe"
			code=$?
			if [ "$code" -ne "$want_exit" ]; then
				echo "FAIL $src: exit $code, want $want_exit"; ok=0
			else
				t0=$(date +%s%N)
				i=0
				while [ "$i" -lt "$RUN_REPS" ]; do
					"$tmp_exe" >/dev/null 2>&1
					i=$((i + 1))
				done
				t1=$(date +%s%N)
				run_ns=$(( (t1 - t0) / RUN_REPS ))
				run_total_ns=$((run_total_ns + run_ns))
			fi
		fi
	fi
	if [ "$ok" -eq 1 ]; then echo "PASS $src (compile $(fmt_ms "$compile_ns"), run $(fmt_ms "$run_ns"))"; passed=$((passed+1)); else failed=$((failed+1)); fi
done
echo "$passed passed, $failed failed (compile $(fmt_ms "$compile_total_ns") total, run $(fmt_ms "$run_total_ns") total, per-run means, gcc excluded)"
[ "$failed" -eq 0 ]

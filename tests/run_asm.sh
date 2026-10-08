#!/bin/sh

# usage: tests/run_asm.sh <r2p> <dir>
# Assembler suite: for each <name>_src.r2, compile to assembly with r2p,
# assemble with gcc, run the binary and check its exit code.
# Expected exit code lives in <name>_exit.txt (default 0).
# Expected compiler stderr lives in <name>_stderr.txt (default: must be empty).
# Differential: each test is compiled with -O0, -O1 and -O2 and must exit
# with the same code (optimizer must preserve semantics).

# Timing: one run of these tiny programs takes <1ms, below the noise floor
# of a single `date` call (~0.5ms fork+exec each). So each test is compiled
# COMPILE_REPS times and executed RUN_REPS times per opt level (-O0/-O1/-O2);
# the reported numbers are per-run means (batch wall time / reps), which
# amortizes timer overhead and scheduler jitter. Override with e.g.
# RUN_REPS=50 COMPILE_REPS=10 tests/run_asm.sh <r2p> <dir> (gcc is untimed).
# Per-test rows show execution means; the footer shows compile+run totals.
# O1 is the default (plain r2p == -O1); O0 = sin optimizar, O2 = full.

bin="$1"; dir="$2"
RUN_REPS=${RUN_REPS:-20}
COMPILE_REPS=${COMPILE_REPS:-5}
passed=0; failed=0
c0_tot=0; c1_tot=0; c2_tot=0
r0_tot=0; r1_tot=0; r2_tot=0
tmp_out=$(mktemp); tmp_err=$(mktemp); tmp_gcc=$(mktemp)
tmp_asm=$(mktemp --suffix=.s); tmp_exe=$(mktemp)
trap 'rm -f "$tmp_out" "$tmp_err" "$tmp_gcc" "$tmp_asm" "$tmp_exe"' EXIT

fmt_ms() { printf "%d.%03dms" $(($1 / 1000000)) $((($1 % 1000000) / 1000)); }
time_compile() {
	# $1 = src, $2 = olev: COMPILE_REPS compiles, echo mean ns.
	t0=$(date +%s%N)
	i=0
	while [ "$i" -lt "$COMPILE_REPS" ]; do
		"$bin" -O$2 "$1" -o "$tmp_asm" >/dev/null 2>&1
		i=$((i + 1))
	done
	t1=$(date +%s%N)
	echo $(( (t1 - t0) / COMPILE_REPS ))
}
time_run() {
	# RUN_REPS executions of $tmp_exe, echo mean ns.
	t0=$(date +%s%N)
	i=0
	while [ "$i" -lt "$RUN_REPS" ]; do
		"$tmp_exe" >/dev/null 2>&1
		i=$((i + 1))
	done
	t1=$(date +%s%N)
	echo $(( (t1 - t0) / RUN_REPS ))
}

printf "%-30s %10s %10s %10s\n" "test" "O0 run" "O1 run" "O2 run"

for src in "$dir"/*_src.r2; do
	[ -e "$src" ] || { echo "No tests found in $dir"; exit 1; }
	base=${src%_src.r2}
	name=${src##*/}; name=${name%_src.r2}
	want_exit=0
	[ -f "${base}_exit.txt" ] && want_exit=$(cat "${base}_exit.txt")
	ok=1
	c0=0; c1=0; c2=0; r0=0; r1=0; r2=0
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
		for olev in 0 1 2; do
			"$bin" -O$olev "$src" -o "$tmp_asm" >/dev/null 2>&1
			if [ $? -ne 0 ]; then
				echo "FAIL $src: r2p -O$olev exited nonzero"; ok=0; break
			fi
			c=$(time_compile "$src" $olev)
			case $olev in 0) c0=$c;; 1) c1=$c;; 2) c2=$c;; esac
			gcc "$tmp_asm" -o "$tmp_exe" 2>"$tmp_gcc"
			if [ $? -ne 0 ]; then
				echo "FAIL $src: gcc -O$olev failed:"; cat "$tmp_gcc"; ok=0; break
			fi
			"$tmp_exe" >/dev/null 2>&1
			code_opt=$?
			if [ "$code_opt" -ne "$want_exit" ]; then
				echo "FAIL $src: -O$olev exit $code_opt, want $want_exit"; ok=0; break
			fi
			r=$(time_run)
			case $olev in 0) r0=$r;; 1) r1=$r;; 2) r2=$r;; esac
		done
	fi
	if [ "$ok" -eq 1 ]; then
		printf "%-30s %10s %10s %10s\n" "$name" \
			"$(fmt_ms "$r0")" "$(fmt_ms "$r1")" "$(fmt_ms "$r2")"
		c0_tot=$((c0_tot + c0)); c1_tot=$((c1_tot + c1)); c2_tot=$((c2_tot + c2))
		r0_tot=$((r0_tot + r0)); r1_tot=$((r1_tot + r1)); r2_tot=$((r2_tot + r2))
		passed=$((passed+1))
	else
		printf "%-30s %s\n" "$name" "FAIL (see above)"
		failed=$((failed+1))
	fi
done
printf "%-30s %10s %10s %10s\n" "TOTAL run" \
	"$(fmt_ms "$r0_tot")" "$(fmt_ms "$r1_tot")" "$(fmt_ms "$r2_tot")"
printf "%-30s %10s %10s %10s\n" "TOTAL compile" \
	"$(fmt_ms "$c0_tot")" "$(fmt_ms "$c1_tot")" "$(fmt_ms "$c2_tot")"
echo "$passed passed, $failed failed (totals are per-run means x tests, gcc excluded)"
[ "$failed" -eq 0 ]

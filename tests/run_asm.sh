#!/bin/sh

# usage: tests/run_asm.sh <r2p> <dir>
# Assembler suite: for each <name>_src.r2, compile to assembly with r2p,
# assemble with gcc, run the binary and check its exit code.
# Expected exit code lives in <name>_exit.txt (default 0).
# Expected compiler stderr lives in <name>_stderr.txt (default: must be empty).
# Differential: each test is compiled with -O0, -O1 and -O2 and must exit
# with the same code (optimizer must preserve semantics).

# Bench: wall-clock timing of these tiny programs (<1ms) sits below the noise
# floor of a single `date` call and depends heavily on the OS scheduler, so
# each test binary is executed once per opt level under valgrind callgrind
# and the reported numbers are deterministic instruction counts, not times.
# Requires valgrind in PATH. (gcc is untimed.)
# Only user code is counted: the dynamic loader and libc startup cost ~130k
# Ir on every run and would drown the signal (an empty main is 2 Ir), so
# collection starts off and is toggled on only inside __r2_main (the user
# main) and __r2.init (global initializers), via --toggle-collect. Callees
# count while inside; loader/libc and the tiny main wrapper stay excluded.
# Per-test rows show run Ir; the footer shows run totals.
# O1 is the default (plain r2p == -O1); O0 = sin optimizar, O2 = full.

bin="$1"; dir="$2"
command -v valgrind >/dev/null 2>&1 || { echo "valgrind not found in PATH"; exit 1; }
passed=0; failed=0
r0_tot=0; r1_tot=0; r2_tot=0
tmp_out=$(mktemp); tmp_err=$(mktemp); tmp_gcc=$(mktemp); tmp_vg=$(mktemp)
tmp_asm=$(mktemp --suffix=.s); tmp_exe=$(mktemp)
trap 'rm -f "$tmp_out" "$tmp_err" "$tmp_gcc" "$tmp_vg" "$tmp_asm" "$tmp_exe"' EXIT

fmt_irefs() { echo "$1" | sed ':a;s/\B[0-9]\{3\}\>/,&/;ta'; }
vg_run() {
	# Single callgrind run of $tmp_exe, collecting only inside __r2_main /
	# __r2.init. Sets VG_COUNT to the raw Ir and returns the program exit
	# code (valgrind propagates it).
	valgrind --tool=callgrind --collect-atstart=no \
		--toggle-collect=__r2_main --toggle-collect='__r2.init' \
		--callgrind-out-file=/dev/null \
		"$tmp_exe" >/dev/null 2>"$tmp_vg"
	code=$?
	VG_COUNT=$(sed -n 's/.*Collected : *//p' "$tmp_vg" | tr -d ' ,')
	return $code
}

printf "%-30s %12s %12s %12s\n" "test" "O0 run" "O1 run" "O2 run"

for src in "$dir"/*_src.r2; do
	[ -e "$src" ] || { echo "No tests found in $dir"; exit 1; }
	base=${src%_src.r2}
	name=${src##*/}; name=${name%_src.r2}
	want_exit=0
	[ -f "${base}_exit.txt" ] && want_exit=$(cat "${base}_exit.txt")
	ok=1
	r0=0; r1=0; r2=0
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
			gcc "$tmp_asm" -o "$tmp_exe" 2>"$tmp_gcc"
			if [ $? -ne 0 ]; then
				echo "FAIL $src: gcc -O$olev failed:"; cat "$tmp_gcc"; ok=0; break
			fi
			vg_run
			code_opt=$?
			case "$VG_COUNT" in ''|*[!0-9]*)
				echo "FAIL $src: -O$olev could not parse Ir:"; cat "$tmp_vg"; ok=0; break;;
			esac
			if [ "$code_opt" -ne "$want_exit" ]; then
				echo "FAIL $src: -O$olev exit $code_opt, want $want_exit"; ok=0; break
			fi
			case $olev in 0) r0=$VG_COUNT;; 1) r1=$VG_COUNT;; 2) r2=$VG_COUNT;; esac
		done
	fi
	if [ "$ok" -eq 1 ]; then
		printf "%-30s %12s %12s %12s\n" "$name" \
			"$(fmt_irefs "$r0")" "$(fmt_irefs "$r1")" "$(fmt_irefs "$r2")"
		r0_tot=$((r0_tot + r0)); r1_tot=$((r1_tot + r1)); r2_tot=$((r2_tot + r2))
		passed=$((passed+1))
	else
		printf "%-30s %s\n" "$name" "FAIL (see above)"
		failed=$((failed+1))
	fi
done
printf "%-30s %12s %12s %12s\n" "TOTAL run" \
	"$(fmt_irefs "$r0_tot")" "$(fmt_irefs "$r1_tot")" "$(fmt_irefs "$r2_tot")"
echo "$passed passed, $failed failed (binary-only Ir via valgrind callgrind, gcc excluded)"
[ "$failed" -eq 0 ]

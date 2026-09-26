#!/bin/sh
# usage: tests/regen_fixtures.sh <r2p>
# Regenerates every tests/<suite>/*_stderr.txt from actual binary output.
# Verifies stdout (_result.txt) and exit status (_status.txt) did not change;
# reports fixtures whose stderr changed by more than ANSI color removal.
bin="$1"
[ -x "$bin" ] || { echo "usage: $0 <path-to-r2p>"; exit 1; }

tmp_out=$(mktemp); tmp_err=$(mktemp); tmp_old=$(mktemp)
trap 'rm -f "$tmp_out" "$tmp_err" "$tmp_old"' EXIT

strip_ansi() { sed 's/\x1b\[[0-9;]*m//g'; }

review=0
for pair in "lexer --dump-tokens" "parser --dump-ast" "sema --dump-symbols" "ir --dump-ir"; do
	suite=${pair%% *}
	flag=${pair##* }
	for src in tests/$suite/*_src.r2; do
		[ -e "$src" ] || continue
		base=${src%_src.r2}

		"$bin" "$flag" "$src" >"$tmp_out" 2>"$tmp_err"
		status=$?

		want_status=0
		[ -f "${base}_status.txt" ] && want_status=$(cat "${base}_status.txt")
		[ "$status" -ne "$want_status" ] && echo "STATUS CHANGED: $src (got $status, want $want_status)"

		if ! diff -q "${base}_result.txt" "$tmp_out" >/dev/null; then
			echo "STDOUT CHANGED: $src"
			review=1
		fi

		if [ -f "${base}_stderr.txt" ]; then
			strip_ansi <"${base}_stderr.txt" >"$tmp_old"
			if ! diff -q "$tmp_old" "$tmp_err" >/dev/null; then
				echo "STDERR CHANGED BEYOND COLORS: $src"
				review=1
			fi
			cp "$tmp_err" "${base}_stderr.txt"
		elif [ -s "$tmp_err" ]; then
			echo "NEW STDERR (no fixture): $src"
			review=1
		fi
	done
done

echo "stderr fixtures regenerated"
[ "$review" -eq 0 ]

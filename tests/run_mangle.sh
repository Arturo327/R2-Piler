#!/bin/sh

# usage: tests/run_mangle.sh <r2p> [fixture-dir]
#
# Fixture-driven mangling/pub/export/symbol suite. Every subdirectory of the
# fixture dir (default: tests/run_mangle next to this script) that contains a
# `test.conf` is one case: the directory is copied into a scratch workspace and
# the directives below drive the compiler and the checks. Adding a case only
# means adding a directory; this script never hardcodes one.
#
# test.conf: `key value` per line, `#` starts a comment line.
#   cwd <dir>             directory to run the compiler from (workspace-relative
#                         or absolute; default ".")
#   source <path>         source file as written on the command line (required)
#   output <path>         assembly output, workspace-relative (default "out.s")
#   status <n>            expected compiler exit status (default 0)
#   stdout <file>         expected stdout, case-relative (default: empty)
#   stderr <file>         expected stderr, case-relative (default: empty)
#   contains <text>       assembly must contain this literal (repeatable)
#   absent <text>         assembly must not contain this literal (repeatable)
#   gcc <file>            C source linked with the assembly (repeatable,
#                         workspace-relative); the resulting program is run
#   run_exit <n>          expected exit status of that program (default 0)
#   symlink <path> <tgt>  create a symlink in the workspace before compiling
#                         (repeatable)
#
# `$ROOT` inside a path expands to the case's scratch workspace, so a case can
# compile an absolute path: `cwd /`, `source $ROOT/proj/src/x.r2`.

bin="$1"
dir="$2"
[ -n "$dir" ] || dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/run_mangle
[ -x "$bin" ] || { echo "usage: $0 <path-to-r2p> [fixture-dir]"; exit 1; }
[ -d "$dir" ] || { echo "No tests found in $dir"; exit 1; }
bin=$(cd "$(dirname "$bin")" && pwd)/$(basename "$bin")

root=$(mktemp -d)
trap 'rm -rf "$root"' EXIT
passed=0; failed=0

pass() { echo "PASS $1"; passed=$((passed+1)); }
fail() { echo "FAIL $1"; failed=$((failed+1)); }

expand() { # text -> $ROOT replaced with the current case workspace
	printf '%s\n' "$1" | sed "s|\$ROOT|$ws|g"
}

show() { # file
	[ -s "$1" ] && sed 's/^/  /' "$1"
	return 0
}

check_stream() { # expected-file actual-file label
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

check_asm() { # asm-file checks-file
	while IFS= read -r line; do
		[ -n "$line" ] || continue
		want=${line%% *}; pat=${line#* }
		if grep -Fq -- "$pat" "$1"; then
			[ "$want" -eq 1 ] && continue
			echo "  unexpected '$pat'"
			return 1
		else
			[ "$want" -eq 0 ] && continue
			echo "  missing '$pat'"
			return 1
		fi
	done <"$2"
	return 0
}

run_case() { # conf
	case_dir=${1%/test.conf}
	name=${case_dir##*/}
	ws="$root/$name"
	mkdir -p "$ws" || { fail "$name"; return 1; }
	cp -R "$case_dir/." "$ws/" || { fail "$name"; return 1; }

	cwd=.; src=; out=out.s; want=0; run_exit=0
	stdout_exp=; stderr_exp=
	checks="$root/$name.checks"; links="$root/$name.links"
	syms="$root/$name.syms"
	: >"$checks"; : >"$links"; : >"$syms"
	ok=1

	while IFS= read -r line || [ -n "$line" ]; do
		case $line in ''|'#'*) continue;; esac
		case $line in
		*" "*) key=${line%% *}; val=${line#* };;
		*) key=$line; val=;;
		esac
		case $key in
		cwd) cwd=$val;;
		source) src=$val;;
		output) out=$val;;
		status) want=$val;;
		stdout) stdout_exp=$val;;
		stderr) stderr_exp=$val;;
		contains) printf '1 %s\n' "$val" >>"$checks";;
		absent) printf '0 %s\n' "$val" >>"$checks";;
		gcc) printf '%s\n' "$val" >>"$links";;
		run_exit) run_exit=$val;;
		symlink) printf '%s\t%s\n' "${val%% *}" "${val#* }" >>"$syms";;
		*) echo "FAIL $name: unknown directive '$key'"; ok=0;;
		esac
	done <"$1"
	[ -n "$src" ] || { echo "FAIL $name: missing 'source'"; ok=0; }
	[ -n "$stdout_exp" ] && stdout_exp="$case_dir/$stdout_exp"
	[ -n "$stderr_exp" ] && stderr_exp="$case_dir/$stderr_exp"

	cwd=$(expand "$cwd"); src=$(expand "$src"); out=$(expand "$out")
	case $out in /*) ;; *) out="$ws/$out";; esac
	case $cwd in /*) run_dir=$cwd;; *) run_dir="$ws/$cwd";; esac

	while IFS='	' read -r link target; do
		[ -n "$link" ] || continue
		link=$(expand "$link"); target=$(expand "$target")
		case $link in /*) ;; *) link="$ws/$link";; esac
		[ "$link" = "${link%/*}" ] || mkdir -p "${link%/*}"
		ln -sf "$target" "$link" || { echo "  cannot symlink $link"; ok=0; }
	done <"$syms"
	[ "$ok" -eq 1 ] || { fail "$name"; return 1; }

	(cd "$run_dir" && "$bin" "$src" -o "$out" >"$root/stdout" 2>"$root/stderr")
	status=$?
	[ "$status" -eq "$want" ] || {
		echo "  status $status, want $want"
		show "$root/stderr"
		ok=0
	}
	check_stream "$stdout_exp" "$root/stdout" stdout || ok=0
	check_stream "$stderr_exp" "$root/stderr" stderr || ok=0

	if [ "$ok" -eq 1 ] && [ "$status" -eq 0 ]; then
		[ -f "$out" ] || { echo "  no assembly at $out"; ok=0; }
		[ "$ok" -eq 0 ] || check_asm "$out" "$checks" || ok=0
	fi

	if [ "$ok" -eq 1 ] && [ -s "$links" ]; then
		set -- "$out"
		while IFS= read -r csrc; do
			[ -n "$csrc" ] || continue
			csrc=$(expand "$csrc")
			case $csrc in /*) ;; *) csrc="$ws/$csrc";; esac
			set -- "$@" "$csrc"
		done <"$links"
		if gcc "$@" -o "$ws/prog" 2>"$root/gcc"; then
			(cd "$ws" && ./prog); code=$?
			[ "$code" -eq "$run_exit" ] || {
				echo "  linked program exited $code, want $run_exit"
				ok=0
			}
		else
			echo "  gcc failed:"
			show "$root/gcc"
			ok=0
		fi
	fi

	if [ "$ok" -eq 1 ]; then pass "$name"; else fail "$name"; fi
}

found=0
for conf in "$dir"/*/test.conf; do
	[ -e "$conf" ] || continue
	found=1
	run_case "$conf"
done
[ "$found" -eq 1 ] || { echo "No tests found in $dir"; exit 1; }

echo "$passed passed, $failed failed"
[ "$failed" -eq 0 ]
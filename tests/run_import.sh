#!/bin/sh

# usage: tests/run_import.sh <r2p> <dir>
#
# Fixture-driven multi-module suite. Every subdirectory of the fixture dir
# (default: tests/run_import next to this script) with a `test.conf` is one
# case: the directory is copied into a scratch workspace and the directives
# below drive one or more compilations and, optionally, a link-and-run.
#
# test.conf: `key value` per line, `#` starts a comment line. Directives are
# processed in order.
#   modules <dir>         directory passed to -m for later builds (default modules)
#   args <text>           extra compiler arguments for later builds
#   no_r2i                add --no-r2i to later builds
#   build <src> [<asm>]   compile <src> (workspace-relative); default output is
#                         <src with .r2 replaced by .s>
#   copy <src> <dst>      copy a workspace file (parents are created)
#   status <n>            expected status of the LAST build (default 0)
#   stderr <file>         expected stderr of the LAST build, case-relative
#   contains <text>       last assembly must contain this literal (repeatable)
#   absent <text>         last assembly must not contain this literal (repeatable)
#   run <asm> [<asm>...]  files to link and run
#   run_exit <n>          expected exit status of the linked program (implies run)
#
# `$ROOT` inside a path or an expected-stderr file expands to the case workspace.

bin="$1"; dir="$2"
[ -n "$dir" ] || dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/run_import
[ -x "$bin" ] || { echo "usage: $0 <path-to-r2p> [fixture-dir]"; exit 1; }
[ -d "$dir" ] || { echo "No tests found in $dir"; exit 1; }
bin=$(cd "$(dirname "$bin")" && pwd)/$(basename "$bin")

root=$(mktemp -d)
trap 'rm -rf "$root"' EXIT
passed=0; failed=0

pass() { echo "PASS $1"; passed=$((passed+1)); }
fail() { echo "FAIL $1"; failed=$((failed+1)); }
show() { [ -s "$1" ] && sed 's/^/  /' "$1"; return 0; }

expand() { # text -> $ROOT replaced with the current case workspace
	printf '%s\n' "$1" | sed "s|\$ROOT|$ws|g"
}

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

check_asm() { # asm-file checks-file
	while IFS= read -r line; do
		[ -n "$line" ] || continue
		want=${line%% *}; pat=${line#* }
		if grep -Fqw -- "$pat" "$1"; then
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

	mdir=modules; extra=; no_r2i=0
	want=0; stderr_exp=; run_list=; run_exit=
	checks="$root/$name.checks"
	: >"$checks"
	build_count=0; last_status=0; last_asm=; earlier_bad=0; builds=
	ok=1

	while IFS= read -r line || [ -n "$line" ]; do
		case $line in ''|'#'*) continue;; esac
		case $line in
		*" "*) key=${line%% *}; val=${line#* };;
		*) key=$line; val=;;
		esac
		case $key in
		modules) mdir=$val;;
		args) extra="$extra $val";;
		no_r2i) no_r2i=1;;
		build) {
			set -- $val
			src=$1; asm=${2:-${src%.r2}.s}
			build_count=$((build_count+1))
			mkdir -p "$ws/$(dirname "$asm")"
			flags="$extra"
			[ "$no_r2i" -eq 1 ] && flags="$flags --no-r2i"
			(cd "$ws" && "$bin" "$src" -m "$mdir" -o "$asm" $flags \
				>"$root/stdout" 2>"$root/build$build_count.err")
			last_status=$?
			echo "$last_status" >"$root/build$build_count.st"
			last_asm=$asm
			if [ "$last_status" -ne 0 ] || [ -s "$root/build$build_count.err" ]; then
				earlier_bad=1
			fi
			builds="$builds $asm"
		};;
		copy) {
			set -- $val
			from=$(expand "$1"); to=$(expand "$2")
			case $from in /*) ;; *) from="$ws/$from";; esac
			case $to in /*) ;; *) to="$ws/$to";; esac
			mkdir -p "$(dirname "$to")"
			cp "$from" "$to" || { echo "  cannot copy $from"; ok=0; }
		};;
		status) want=$val;;
		stderr) stderr_exp="$case_dir/$val";;
		contains) printf '1 %s\n' "$val" >>"$checks";;
		absent) printf '0 %s\n' "$val" >>"$checks";;
		run) run_list=$(expand "$val");;
		run_exit) run_exit=$val;;
		*) echo "FAIL $name: unknown directive '$key'"; ok=0;;
		esac
	done <"$1"

	[ "$build_count" -gt 0 ] || { echo "FAIL $name: no build"; ok=0; }
	[ "$last_status" -eq "$want" ] || {
		echo "  last build status $last_status, want $want"
		show "$root/build$build_count.err"
		ok=0
	}
	check_err "$stderr_exp" "$root/build$build_count.err" stderr || ok=0
	if [ "$earlier_bad" -ne 0 ]; then
		n=1
		while [ "$n" -lt "$build_count" ]; do
			code=$(cat "$root/build$n.st" 2>/dev/null)
			if [ "$code" != 0 ] || [ -s "$root/build$n.err" ]; then
				echo "  earlier build $n failed or warned:"
				show "$root/build$n.err"
				ok=0
			fi
			n=$((n + 1))
		done
	fi

	if [ "$ok" -eq 1 ] && [ "$last_status" -eq 0 ] && [ -n "$last_asm" ]; then
		[ -f "$ws/$last_asm" ] || { echo "  no assembly at $last_asm"; ok=0; }
		[ "$ok" -eq 0 ] || check_asm "$ws/$last_asm" "$checks" || ok=0
	fi

	if [ "$ok" -eq 1 ] && [ -n "$run_list" ]; then
		want_exit=${run_exit:-0}
		if (cd "$ws" && gcc $run_list -o prog 2>"$root/gcc"); then
			(cd "$ws" && ./prog); code=$?
			[ "$code" -eq "$want_exit" ] || {
				echo "  linked program exited $code, want $want_exit"
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
#!/bin/sh

# usage: tests/run_mangle.sh <r2p>
# Checks module-name resolution and visibility directives: pub/private symbols,
# paths relative to the project root, ../ folding, absolute-path fallback and
# exported symbols linked from C.

bin="$1"
[ -x "$bin" ] || { echo "usage: $0 <path-to-r2p>"; exit 1; }
bin=$(cd "$(dirname "$bin")" && pwd)/$(basename "$bin")
root=$(mktemp -d)
trap 'rm -rf "$root"' EXIT
passed=0; failed=0

pass() { echo "PASS $1"; passed=$((passed+1)); }
fail() { echo "FAIL $1"; failed=$((failed+1)); }

have_symbols() { # label file pattern...
	label=$1; file=$2; shift 2
	for pat in "$@"; do
		if ! grep -Fq "$pat" "$file"; then
			echo "  $label: missing '$pat'"
			return 1
		fi
	done
	return 0
}

check_symbols() { # label file pattern...
	label=$1; file=$2; shift 2
	if have_symbols "$label" "$file" "$@"; then pass "$label"; else fail "$label"; fi
}

compile() { # dir source output
	(cd "$1" && "$bin" "$2" -o "$3" 2>"$root/stderr")
	if [ $? -ne 0 ]; then
		echo "  compile failed: $2"
		cat "$root/stderr"
		return 1
	fi
	return 0
}

mkdir -p "$root/proj/src" "$root/proj/build"
cat >"$root/proj/src/math.r2" <<'EOF'
pub fn add (a : i64, b : i64) : i64 { return a + b; }
fn hidden () : i64 { return 1; }
pub var count : i64 = 3;
var secret : i64 = 4;
pub fn main () : i64 { return 0; }
EOF
cat >"$root/proj/src/weird-name.r2" <<'EOF'
pub fn f () : i64 { return 0; }
EOF
cat >"$root/proj/code.r2" <<'EOF'
export fn callback (x : i64) : i64 { return x + 1; }
export var contador : i64 = 5;
EOF
cat >"$root/main.c" <<'EOF'
#include <stdint.h>
extern int64_t callback (int64_t x);
extern int64_t contador;
int main (void) { return (int)callback(contador); }
EOF

if compile "$root/proj" src/math.r2 "$root/relative.s"; then
	check_symbols "relative path and pub visibility" "$root/relative.s" \
		'.globl _r2_pf_3src4math_3add' \
		'.local _r2_f_3src4math_6hidden' \
		'.globl _r2_pv_3src4math_5count' \
		'.local _r2_v_3src4math_6secret' \
		'.globl _r2_pf_3src4math_4main'
else
	fail "relative path and pub visibility"
fi

if compile "$root/proj/build" ../src/math.r2 "$root/parent.s"; then
	check_symbols "parent path keeps the project root" "$root/parent.s" \
		'.globl _r2_pf_3src4math_3add' \
		'.local _r2_f_3src4math_6hidden'
else
	fail "parent path keeps the project root"
fi

if (cd / && "$bin" "$root/proj/src/math.r2" -o "$root/absolute.s" 2>"$root/stderr"); then
	check_symbols "absolute path falls back to the basename" "$root/absolute.s" \
		'.globl _r2_pf_4math_3add' \
		'.local _r2_f_4math_6hidden'
else
	echo "  compile failed: absolute path"
	cat "$root/stderr"
	fail "absolute path falls back to the basename"
fi

if compile "$root/proj" src/weird-name.r2 "$root/escaped.s"; then
	check_symbols "unsafe segment bytes are escaped" "$root/escaped.s" \
		'.globl _r2_pf_3src12weird$2dname_1f'
else
	fail "unsafe segment bytes are escaped"
fi

if compile "$root/proj" code.r2 "$root/code.s"; then
	ok=1
	have_symbols "export uses plain symbols" "$root/code.s" \
		'.globl callback' 'callback:' \
		'.globl contador' 'contador:' || ok=0
	if grep -Fq '_r2_pf_3src4code_8callback' "$root/code.s"; then
		echo "  export uses plain symbols: found a mangled alias"
		ok=0
	fi
	if [ "$ok" -eq 1 ]; then pass "export uses plain symbols"; else
		fail "export uses plain symbols"
	fi
	if gcc "$root/code.s" "$root/main.c" -o "$root/prog" 2>"$root/stderr"; then
		"$root/prog"; code=$?
		if [ "$code" -eq 6 ]; then pass "export links from C"; else
			echo "  C program exited $code, want 6"
			fail "export links from C"
		fi
	else
		echo "  gcc failed:"
		cat "$root/stderr"
		fail "export links from C"
	fi
else
	fail "export uses plain symbols"
	fail "export links from C"
fi

echo "$passed passed, $failed failed"
[ "$failed" -eq 0 ]

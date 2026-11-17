#!/usr/bin/env bash
# uso: run_test.sh <topograf> <programa> <código de saída esperado> [flags do topograf...]
# Cada linha "// espera: <texto>" do programa deve aparecer na saída (stdout + stderr).
set -u
topograf=$1; prog=$2; expected_rc=$3; shift 3

out=$("$topograf" "$@" "$prog" 2>&1); rc=$?
status=0

if [ "$rc" -ne "$expected_rc" ]; then
    echo "FALHA: código de saída $rc, esperado $expected_rc"
    status=1
fi

while IFS= read -r exp; do
    if ! grep -qF -- "$exp" <<< "$out"; then
        echo "FALHA: não encontrado na saída: $exp"
        status=1
    fi
done < <(sed -n 's|^// espera: ||p' "$prog")

if [ "$status" -ne 0 ]; then
    echo "--- saída ---"
    echo "$out"
fi
exit "$status"

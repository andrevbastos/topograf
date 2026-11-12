#!/usr/bin/env bash
# uso: run_compile.sh <perlang> <programa>
# Compila o programa (-o), roda o executável e o interpretador com --no-display em
# diretórios separados e exige: mesmo código de saída e arquivos idênticos byte a byte.
# Linhas "// espera: <texto>" também são procuradas na saída do executável.
set -u
perlang=$1; prog=$(realpath "$2")
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/compilado" "$tmp/interpretado"
status=0

if ! "$perlang" "$prog" -o "$tmp/prog" > "$tmp/compilacao.txt" 2>&1; then
    echo "FALHA: compilação"; cat "$tmp/compilacao.txt"; exit 1
fi

out=$(cd "$tmp/compilado" && "$tmp/prog" --no-display 2>&1); rc_c=$?
(cd "$tmp/interpretado" && "$perlang" "$prog" --no-display > /dev/null 2>&1); rc_i=$?

[ "$rc_c" -eq "$rc_i" ] || { echo "FALHA: código $rc_c (compilado) != $rc_i (interpretado)"; status=1; }

if ! diff -r "$tmp/compilado" "$tmp/interpretado" > "$tmp/diff.txt"; then
    echo "FALHA: arquivos diferentes"; cat "$tmp/diff.txt"; status=1
fi
n=$(find "$tmp/compilado" -type f | wc -l)
echo "arquivos comparados: $n"

while IFS= read -r exp; do
    grep -qF -- "$exp" <<< "$out" || { echo "FALHA: não encontrado na saída: $exp"; status=1; }
done < <(sed -n 's|^// espera: ||p' "$prog")

[ "$status" -eq 0 ] || { echo "--- saída do executável ---"; echo "$out"; }
exit "$status"

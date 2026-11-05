#!/usr/bin/env bash
# uso: run_export.sh <perlang> <programa>
# Roda o programa com --no-display num diretório temporário e confere:
#   // arquivo: <caminho> <W>x<H>   PNG em cinza gerado com essas dimensões
#   // sem-arquivo: <caminho>       arquivo que não pode existir
#   // espera: <texto>              texto na saída
#   // codigo: <n>                  código de saída esperado (padrão 0)
set -u
perlang=$1; prog=$(realpath "$2")
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
cd "$tmp"

out=$("$perlang" --no-display "$prog" 2>&1); rc=$?
expected_rc=$(sed -n 's|^// codigo: ||p' "$prog"); expected_rc=${expected_rc:-0}
status=0

[ "$rc" -eq "$expected_rc" ] || { echo "FALHA: código $rc, esperado $expected_rc"; status=1; }

while read -r f dims; do
    w=${dims%x*}; h=${dims#*x}
    if ! file "$f" 2>/dev/null | grep -q "PNG image data, $w x $h, 8-bit grayscale"; then
        echo "FALHA: $f não é PNG cinza ${w}x${h}: $(file "$f" 2>&1)"; status=1
    fi
done < <(sed -n 's|^// arquivo: ||p' "$prog")

while read -r f; do
    [ ! -e "$f" ] || { echo "FALHA: $f não deveria existir"; status=1; }
done < <(sed -n 's|^// sem-arquivo: ||p' "$prog")

while IFS= read -r exp; do
    grep -qF -- "$exp" <<< "$out" || { echo "FALHA: não encontrado na saída: $exp"; status=1; }
done < <(sed -n 's|^// espera: ||p' "$prog")

[ "$status" -eq 0 ] || { echo "--- saída ---"; echo "$out"; }
exit "$status"

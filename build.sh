#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"

echo "[*] Checking tools..."
for c in gcc ld objcopy python3; do
    command -v "$c" >/dev/null || { echo "MISSING: $c"; exit 1; }
done

[ -f linker.ld ] || { echo "ERROR: linker.ld is missing from the repo root"; exit 1; }
[ -d src ]       || { echo "ERROR: src/ is missing"; exit 1; }
ls src/*.c >/dev/null 2>&1 || { echo "ERROR: no .c files in src/"; exit 1; }

echo "[*] Clean"
make clean

echo "[*] Building"
make -j"$(nproc)"

echo "[*] Hex"
make hex

echo ""
echo "[+] Built:"
ls -lh toolbox.elf toolbox.bin toolbox.hex
echo ""
echo "Next: paste the contents of toolbox.hex into the 'sc' string in toolbox.lua,"
echo "then: python3 toolbox_launcher.py <PS5_IP>"

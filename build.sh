#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"

echo "[*] Checking tools..."
for c in gcc ld objcopy python3; do
    command -v "$c" >/dev/null || { echo "MISSING: $c"; exit 1; }
done

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

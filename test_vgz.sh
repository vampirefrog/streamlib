#!/bin/bash

echo "=== .vgz File Verification Across All ZIP Archives ==="
echo

cd /home/vampi/Code/streamiolib

total_vgz=0
verified_vgz=0

for zipfile in *.zip; do
    echo "Checking: $zipfile"

    # Count .vgz files
    vgz_files=$(./build/examples/walk_tree "$zipfile" --expand-archives --decompress 2>/dev/null | grep -a -c "\.vgz")

    # Count files starting with "Vgm " (use grep -a to treat as text)
    vgm_headers=$(./build/examples/walk_tree "$zipfile" --expand-archives --decompress --show-content 2>/dev/null | grep -a "\.vgz" -A1 | grep -a -c 'Content: "Vgm')

    echo "  .vgz files: $vgz_files"
    echo "  With 'Vgm ' header: $vgm_headers"

    if [ "$vgz_files" -eq "$vgm_headers" ] && [ "$vgz_files" -gt 0 ]; then
        echo "  ✓ ALL VERIFIED"
    else
        echo "  ✗ MISMATCH OR NO FILES"
    fi

    total_vgz=$((total_vgz + vgz_files))
    verified_vgz=$((verified_vgz + vgm_headers))

    echo
done

echo "=== SUMMARY ==="
echo "Total .vgz files found: $total_vgz"
echo "Verified with 'Vgm ' header: $verified_vgz"

if [ "$total_vgz" -eq "$verified_vgz" ]; then
    echo "✓ ALL .VGZ FILES VERIFIED SUCCESSFULLY"
else
    echo "✗ VERIFICATION FAILED"
fi

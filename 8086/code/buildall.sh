#!/bin/sh
# Assemble every program and report failures.
ok=0; bad=0
for f in ch*/*.asm; do
    if nasm -f bin -I. "$f" -o "${f%.asm}.com" 2>/tmp/nasm.err; then
        ok=$((ok+1))
    else
        bad=$((bad+1))
        echo "FAILED: $f"
        sed 's/^/    /' /tmp/nasm.err
    fi
done
echo
echo "$ok assembled, $bad failed"

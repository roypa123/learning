#!/bin/sh
# Usage: ./build.sh ch34/hello
set -e
[ -n "$1" ] || { echo "usage: $0 <path/to/source-without-extension>"; exit 1; }
nasm -f bin -I. "$1.asm" -o "$1.com" -l "$1.lst"
echo "Built $1.com  ($(wc -c < "$1.com") bytes)"

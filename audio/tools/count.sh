#!/usr/bin/env bash
# Running page count for the book. Convention: 400 words = 1 page.
cd "$(dirname "$0")/.." || exit 1
total=0
printf "%-58s %8s %6s\n" "CHAPTER" "WORDS" "PAGES"
echo "--------------------------------------------------------------------------"
for f in book/*.md; do
  [ -e "$f" ] || continue
  w=$(wc -w < "$f")
  total=$((total + w))
  printf "%-58s %8d %6d\n" "$(basename "$f")" "$w" "$((w / 400))"
done
echo "--------------------------------------------------------------------------"
printf "%-58s %8d %6d\n" "TOTAL" "$total" "$((total / 400))"
echo "Target: 700 pages (280000 words)"

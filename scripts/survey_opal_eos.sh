#!/bin/bash
# Two-level inventory of /eos/experiment/opal using the eos.tsize xattr
# (instant) rather than du (which would walk 84 TB).
sz() { timeout 25 getfattr --only-values -n eos.tsize "$1" 2>/dev/null || echo 0; }
nf() { timeout 25 getfattr --only-values -n eos.nfiles "$1" 2>/dev/null || echo ""; }
for d in /eos/experiment/opal/*/; do
  b=$(basename "$d"); s=$(sz "$d")
  printf "TOP\t%s\t%s\n" "$b" "${s:-0}"
  # one level down, capped so a huge directory cannot stall the survey
  timeout 60 ls "$d" 2>/dev/null | head -40 | while read -r sub; do
    [ -d "$d$sub" ] || continue
    ss=$(sz "$d$sub")
    printf "SUB\t%s/%s\t%s\n" "$b" "$sub" "${ss:-0}"
  done
done

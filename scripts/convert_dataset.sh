#!/bin/bash
# Serial conversion of a whole OPAL ntuple directory.
#
# Deliberately serial and resumable: the source is ~32 GB across 359 files and
# the conversion hosts are small, so files are processed one at a time and
# already-converted outputs are skipped on a re-run.
#
# usage: scripts/convert_dataset.sh <indir> <outdir> [options]
#   --stage STAGE    hbook   : only .histo -> plain ROOT (h2root)
#                    edm4hep : only plain ROOT -> EDM4hep (needs stage 1 done)
#                    both    : the full pipeline (default)
#   --pattern GLOB   only files matching GLOB (default '*.histo')
#   --limit N        stop after N files
#   --drop-plain     delete the intermediate plain ROOT after EDM4hep is written
#                    (halves the footprint when both do not fit on disk)
#   --min-free GIB   abort before a file if free space drops below GIB (default 5)
set -euo pipefail

indir=""; outdir=""; pattern='*.histo'; limit=0; drop_plain=0; min_free=5; stage=both
args=()
while [ $# -gt 0 ]; do
  case "$1" in
    --stage) stage=$2; shift 2;;
    --pattern) pattern=$2; shift 2;;
    --limit) limit=$2; shift 2;;
    --drop-plain) drop_plain=1; shift;;
    --min-free) min_free=$2; shift 2;;
    *) args+=("$1"); shift;;
  esac
done
[ ${#args[@]} -ge 2 ] || { echo "usage: $0 <indir> <outdir> [options]" >&2; exit 1; }
indir=${args[0]}; outdir=${args[1]}
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
mkdir -p "$outdir"
log="${outdir}/convert.log"

mapfile -t files < <(find "$indir" -maxdepth 1 -name "$pattern" | sort)
[ ${#files[@]} -gt 0 ] || { echo "no files matching $pattern in $indir" >&2; exit 1; }
[ "$limit" -gt 0 ] && files=("${files[@]:0:$limit}")

case "$stage" in hbook|edm4hep|both) ;; *) echo "bad --stage: $stage" >&2; exit 1;; esac
echo "converting ${#files[@]} files (stage=$stage): $indir -> $outdir" | tee -a "$log"
n=0
for f in "${files[@]}"; do
  n=$((n+1))
  stem=$(basename "$f" .histo)
  plain="${outdir}/${stem}.root"
  if [ "$stage" = hbook ]; then target="$plain"; else target="${outdir}/${stem}.edm4hep.root"; fi
  if [ -s "$target" ]; then
    echo "[$n/${#files[@]}] skip $stem (already converted)" | tee -a "$log"
    continue
  fi

  free_gib=$(df -BG --output=avail "$outdir" | tail -1 | tr -dc '0-9')
  if [ "$free_gib" -lt "$min_free" ]; then
    echo "ABORT: only ${free_gib} GiB free in $outdir (--min-free ${min_free})" | tee -a "$log"
    exit 1
  fi

  echo "[$n/${#files[@]}] $stem (${free_gib} GiB free)" | tee -a "$log"
  ok=1
  case "$stage" in
    hbook)
      "${here}/hbook2root.sh" "$f" "$plain" >>"$log" 2>&1 || ok=0
      ;;
    edm4hep)
      if [ ! -s "$plain" ]; then
        echo "  SKIP: $stem has no plain ROOT yet (run --stage hbook first)" | tee -a "$log"
        continue
      fi
      ( source "${here}/setup_env.sh" key4hep
        "${OPAL_PASS_BIN:-${here}/../build/opal_ntuple_pass}" --rntuple -q \
            "$plain" "${outdir}/${stem}.edm4hep.root" ) >>"$log" 2>&1 || ok=0
      ;;
    both)
      "${here}/convert_file.sh" "$f" "$outdir" >>"$log" 2>&1 || ok=0
      ;;
  esac
  if [ "$ok" = 0 ]; then
    echo "  FAILED: $stem -- see $log" | tee -a "$log"
    continue
  fi
  [ "$drop_plain" = 1 ] && [ "$stage" != hbook ] && rm -f "$plain"
done
if [ "$stage" = hbook ]; then
  echo "done: $(ls -1 "$outdir"/*.root 2>/dev/null | grep -vc edm4hep || true) plain ROOT files" | tee -a "$log"
else
  echo "done: $(ls -1 "$outdir"/*.edm4hep.root 2>/dev/null | wc -l) EDM4hep files" | tee -a "$log"
fi

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
#   --max-rss MB     warn if any file's peak RSS exceeds MB (default 2000)
#
# Peak RSS and wall time per file are recorded to <outdir>/rss.log. The host has
# ~6 GB and no swap, so this is a real guard rail, not decoration.
set -euo pipefail

indir=""; outdir=""; pattern='*.histo'; limit=0; drop_plain=0; min_free=5; stage=both; max_rss=2000
args=()
while [ $# -gt 0 ]; do
  case "$1" in
    --stage) stage=$2; shift 2;;
    --pattern) pattern=$2; shift 2;;
    --limit) limit=$2; shift 2;;
    --drop-plain) drop_plain=1; shift;;
    --min-free) min_free=$2; shift 2;;
    --max-rss) max_rss=$2; shift 2;;
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
rss_log="${outdir}/rss.log"
rss_tmp=$(mktemp); trap 'rm -f "$rss_tmp"' EXIT
max_seen=0; total_secs=0
[ -s "$rss_log" ] || echo "# file peak_rss_mb wall_s out_mb" > "$rss_log"

echo "converting ${#files[@]} files (stage=$stage): $indir -> $outdir" | tee -a "$log"
echo "RSS log: $rss_log" | tee -a "$log"
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
  if [ "$stage" = edm4hep ] && [ ! -s "$plain" ]; then
    echo "  SKIP: $stem has no plain ROOT yet (run --stage hbook first)" | tee -a "$log"
    continue
  fi

  ok=1
  case "$stage" in
    hbook)
      /usr/bin/time -f "%M %e" -o "$rss_tmp" \
        "${here}/hbook2root.sh" "$f" "$plain" >>"$log" 2>&1 || ok=0
      ;;
    edm4hep)
      /usr/bin/time -f "%M %e" -o "$rss_tmp" bash -c '
        source "$1/setup_env.sh" key4hep
        exec "${OPAL_PASS_BIN:-$1/../build/opal_ntuple_pass}" --rntuple -q "$2" "$3"
      ' _ "$here" "$plain" "${outdir}/${stem}.edm4hep.root" >>"$log" 2>&1 || ok=0
      ;;
    both)
      /usr/bin/time -f "%M %e" -o "$rss_tmp" \
        "${here}/convert_file.sh" "$f" "$outdir" >>"$log" 2>&1 || ok=0
      ;;
  esac

  peak_mb=0; secs=0
  if [ -s "$rss_tmp" ]; then
    read -r peak_kb secs _ < <(tail -1 "$rss_tmp")
    peak_mb=$(( ${peak_kb:-0} / 1024 ))
  fi
  [ "$peak_mb" -gt "$max_seen" ] && max_seen=$peak_mb
  total_secs=$(awk -v a="$total_secs" -v b="${secs:-0}" 'BEGIN{printf "%.0f", a+b}')

  if [ "$ok" = 0 ]; then
    echo "  FAILED: $stem -- see $log" | tee -a "$log"
    continue
  fi

  if [ "$stage" = hbook ]; then produced="$plain"; else produced="${outdir}/${stem}.edm4hep.root"; fi
  out_mb=$(( $(stat -c%s "$produced" 2>/dev/null || echo 0) / 1048576 ))
  echo "$stem $peak_mb ${secs:-0} $out_mb" >> "$rss_log"
  echo "      RSS ${peak_mb} MB (max ${max_seen} MB), ${secs}s, out ${out_mb} MB" | tee -a "$log"
  if [ "$peak_mb" -gt "$max_rss" ]; then
    echo "  WARNING: peak RSS ${peak_mb} MB exceeds --max-rss ${max_rss} MB" | tee -a "$log"
  fi
  [ "$drop_plain" = 1 ] && [ "$stage" != hbook ] && rm -f "$plain"
done
if [ "$stage" = hbook ]; then
  echo "done: $(ls -1 "$outdir"/*.root 2>/dev/null | grep -vc edm4hep || true) plain ROOT files" | tee -a "$log"
else
  echo "done: $(ls -1 "$outdir"/*.edm4hep.root 2>/dev/null | wc -l) EDM4hep files" | tee -a "$log"
fi
echo "peak RSS across all files: ${max_seen} MB; total ${total_secs}s; $(du -sh "$outdir" 2>/dev/null | cut -f1) written" | tee -a "$log"

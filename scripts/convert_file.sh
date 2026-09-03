#!/bin/bash
# Full pipeline for one OPAL ntuple file, producing BOTH deliverables:
#   <outdir>/<stem>.root          plain ROOT tree straight from h2root
#   <outdir>/<stem>.edm4hep.root  EDM4hep (RNTuple, ZSTD, no jet variables)
#
# usage: scripts/convert_file.sh <input.histo> <outdir> [extra opal_ntuple_pass args]
set -euo pipefail

[ $# -ge 2 ] || { echo "usage: $0 <input.histo> <outdir> [pass-args...]" >&2; exit 1; }
in=$1; outdir=$2; shift 2
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
root_dir=$(cd "${here}/.." && pwd)
stem=$(basename "$in" .histo)
mkdir -p "$outdir"

plain="${outdir}/${stem}.root"
edm4hep="${outdir}/${stem}.edm4hep.root"

pass="${OPAL_PASS_BIN:-${root_dir}/build/opal_ntuple_pass}"
[ -x "$pass" ] || { echo "converter not built: $pass" >&2; exit 1; }

# Stage 1 and stage 2 run in separate subshells: their environments conflict.
if [ ! -s "$plain" ]; then
  ( "${here}/hbook2root.sh" "$in" "$plain" ) >/dev/null
fi

(
  source "${here}/setup_env.sh" key4hep
  "$pass" --rntuple "$@" "$plain" "$edm4hep"
)

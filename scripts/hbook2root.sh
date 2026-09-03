#!/bin/bash
# Converts one OPAL .histo file (HBOOK RZ, column-wise ntuple QQNT200) into a
# plain ROOT file with the h10 TTree plus the ~115 bundled histograms.
#
# usage: scripts/hbook2root.sh <input.histo> <output.root> [compress-level]
set -euo pipefail

[ $# -ge 2 ] || { echo "usage: $0 <input.histo> <output.root> [compress]" >&2; exit 1; }
in=$1; out=$2; compress=${3:-1}
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

# h2root's ZEBRA store is a fixed ~300-400 MB allocation regardless of input
# size, so files of any size convert in bounded memory.
source "${here}/setup_env.sh" hbook
exec h2root "$in" "$out" "$compress"

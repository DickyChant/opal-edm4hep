#!/bin/bash
# Sources one of the two environments this pipeline needs.
#
# They CANNOT share a shell: h2root lives in an LCG view built against CERNLIB,
# the converter needs the key4hep stack, and sourcing both breaks the compiler
# and library paths for whichever came second. Every script here therefore runs
# each stage in its own subshell.
#
# usage: source scripts/setup_env.sh {hbook|key4hep}

OPAL_LCG_VIEW="${OPAL_LCG_VIEW:-/cvmfs/sft.cern.ch/lcg/views/LCG_107/x86_64-el9-gcc11-opt}"
OPAL_KEY4HEP_SETUP="${OPAL_KEY4HEP_SETUP:-/cvmfs/sw.hsf.org/key4hep/setup.sh}"

# Neither cvmfs setup script is safe under `set -u`/`set -e`; relax while they
# run and restore the caller's settings afterwards.
_opal_saved_opts=$(set +o)
set +u +e

case "$1" in
  hbook)
    source "${OPAL_LCG_VIEW}/setup.sh"
    if ! command -v h2root >/dev/null; then
      echo "h2root not found in ${OPAL_LCG_VIEW}" >&2
      eval "$_opal_saved_opts"; return 1
    fi
    ;;
  key4hep)
    source "${OPAL_KEY4HEP_SETUP}"
    # The LCG/DELPHI-style environments inject flags that break cmake here.
    unset CXXFLAGS CFLAGS LDFLAGS
    ;;
  *)
    echo "usage: source scripts/setup_env.sh {hbook|key4hep}" >&2
    eval "$_opal_saved_opts"; return 1
    ;;
esac

eval "$_opal_saved_opts"
unset _opal_saved_opts

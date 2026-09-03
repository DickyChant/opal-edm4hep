# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A two-stage converter from the OPAL QCD ntuples (`/eos/experiment/opal/ntuple/qcd/`,
31.7 GiB, 359 files) to EDM4hep. Both stage outputs are deliverables: the plain
`h2root` tree and the EDM4hep file.

## The two environments do not mix

This is the single most important structural fact about the repo.

- **`h2root`** (stage 1) lives only in an LCG view, because it needs CERNLIB at
  ROOT build time: `/cvmfs/sft.cern.ch/lcg/views/LCG_107/x86_64-el9-gcc11-opt`.
  It is *not* in the key4hep ROOT, nor in `/usr/bin/root`.
- **The converter** (stage 2) needs the key4hep stack:
  `/cvmfs/sw.hsf.org/key4hep/setup.sh`.

Sourcing both in one shell breaks whichever came second. Every script runs each
stage in its own subshell via `scripts/setup_env.sh {hbook|key4hep}`; keep that
pattern. Neither cvmfs setup script survives `set -u`, which is why
`setup_env.sh` saves and restores shell options around them.

`unset CXXFLAGS CFLAGS LDFLAGS` before `cmake` — sourced environments inject
flags that break the build.

## Commands

```sh
source scripts/setup_env.sh key4hep
cmake -S opal_edm4hep -B build && cmake --build build -j2   # -j2, not -j: see below
ctest --test-dir build                    # unit tests; no input file needed
ctest --test-dir build -R roundtrip       # needs OPAL_TEST_INPUT=<a plain .root>

scripts/convert_file.sh <in.histo> <outdir>                       # both stages
scripts/convert_dataset.sh <indir> <outdir> --stage hbook         # stage 1 only
scripts/convert_dataset.sh <indir> <outdir> --stage edm4hep       # stage 2 only
python3 scripts/validate.py <plain.root> <converted.edm4hep.root> # under key4hep
```

## Memory discipline is a hard constraint

The work host has ~6 GB RAM and **no swap**, against 31.7 GiB of input. This
shaped the design and must not be undone:

- `NtupleReader` uses `TTreeReader` event-at-a-time. Never `RDataFrame` with
  materialised columns, never `tree.arrays()` / `uproot` whole-branch reads.
- `convert_dataset.sh` is strictly serial. Do not parallelise across files.
- Build with `-j2`; a full `-j` OOMs the box.
- Measured peaks: `h2root` ~400 MB (fixed ZEBRA store, independent of input
  size), `opal_ntuple_pass` ~550 MB (RNTuple) / ~700 MB (TTree).

## Architecture

Stage 1 (`scripts/hbook2root.sh`) is a thin `h2root` wrapper: HBOOK RZ ->
TTree `h10`. Stage 2 (`opal_ntuple_pass`) reads `h10` and writes podio.

Three pieces carry the conversion:

- **`NtupleReader`** binds the ~204 branches. Truth branches are optional and
  bound only when `Ntrkh` exists (MC). Its accessors are `const` while the
  `TTreeReaderValue` members are `mutable`, because `operator*` is non-const.
- **`Converter`** builds the EDM4hep collections. The `ReconstructedParticles`
  identity — *all tracks + all clusters not in `Imtkil`*, with MT scale factors —
  is asserted by the validator; keep them in sync.
- **`ScalarPassthrough`** binds, generically and by leaf type, every branch the
  structured conversion does not consume, so a variable added to a future ntuple
  version is carried across without a code change. `kStructuredBranches` in
  `bin/opal_ntuple_pass.cpp` is the exclusion list; a branch added to the
  structured mapping **must** be added there or it will be written twice.

Event-level variables go out as `UserDataCollection` columns with names written
once to the `metadata` frame. Do not move them to podio `GenericParameters`:
that repeats every key string in every event and was measured at a large
fraction of the file.

## Things that were tried and did not work

- `gEnv->SetValue("Root.CompressionAlgorithm", ...)` is **not** honoured for the
  file `podio::ROOTWriter` opens internally; the driver deliberately exposes no
  compression flag rather than one that silently does nothing. Use `--rntuple`
  (ZSTD in one pass, smallest output) or recompress with `hadd -f509`/`-f209`.
- `find_package(EDM4HEP 0.99)` is rejected by EDM4hep 1.0's version check;
  require `1.0`.
- `TTreeReader` needs `ROOT::TreePlayer` linked, not just `Core RIO Tree`.

## Data conventions

Established from the files themselves, not from OPAL documentation — see
[`docs/ntuple-schema.md`](docs/ntuple-schema.md) for the evidence behind each.
The load-bearing ones: `Ichg` is a 0/1 flag so `q = 2*Ichg - 1`; lengths are in
**cm** and EDM4hep wants **mm**; array indices are **1-based**; `Iluch`/`Iluc`
are PDG codes.

**Known issue, now scoped:** the MC hadron-level block sums to ~1.28x `sqrt(s)`
— but **only in the LEP2 four-fermion samples** (`Ievtyp` 4 and 7). At the Z
peak the ratio is 1.000, so the 1.33 M-event LEP1 truth is sound. Carried
through unmodified and reported by the validator. Do not "fix" it by rescaling
or filtering: the 4-vectors are individually self-consistent, and filtering
entries above `Ebeam` undershoots to 0.45.

The dataset spans **both eras** — 42 files at 91 GeV, the rest 130-207 GeV.
`da91_*`/`da1999`/`da2000` are named by running *year*, not energy, so bin by
the per-event `Ebeam`, never by filename.

## Scope

Jets are deliberately out of scope. The DAJETS block is excluded from the
output by default (`--jets` re-enables the variables); no jet collection is
built. It is the largest event-level payload, so this is also the main size
lever.

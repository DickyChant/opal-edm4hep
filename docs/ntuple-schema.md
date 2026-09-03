# The OPAL QCD ntuple, as measured

Everything here was established by reading the files at
`/eos/experiment/opal/ntuple/qcd/`, not from OPAL documentation (which we do
not have). Where a convention was inferred, the evidence is given so it can be
re-checked or corrected.

## Container format

The `.histo` files are **HBOOK RZ** files, not ROOT files. Each holds:

- one column-wise ntuple (CWN), HBOOK ID 10, named `QQNT200` in directory
  `//HISTO`, and
- ~115 `TH1F` histograms of the standard QCD distributions.

They are read with `h2root` from an LCG view (CERNLIB is required at ROOT build
time, so a plain ROOT install will not do). `h2root` writes a TTree called
`h10`. Its ZEBRA store is a fixed ~300-400 MB allocation, so peak memory does
not grow with input size.

## Blocks

| Block | Contents |
| --- | --- |
| `DAGNRL` | run/event id, trigger and detector-status bits, `Ebeam`, thrust axis |
| `DAXTRA` | WW/4-fermion selection variables, ISR-photon and fit quantities |
| `DAEVSH` | event shapes for four object definitions: `DTC` tracks+clusters, `DT` tracks, `DC` clusters, `DMT` MT package |
| `DAJETS` | jet multiplicities and Durham/E0/Cambridge y-resolution scales |
| `DACTRK` | charged tracks |
| `DACLUS` | calorimeter clusters and the MT matching/scaling package |
| `BTVAR` | b-tagging: primary vertex, secondary vertices, network output |
| truth (MC only) | primary fermions, parton level (`*p`), hadron level (`*h`), per-track truth match |

Data files carry 148 branches, MC 204; **the MC schema is a strict superset**,
so one reader handles both. `Ntrkh` is the discriminator.

## Conventions established from the data

| Convention | Evidence |
| --- | --- |
| `Ichg` is a **0/1 flag**, not a signed charge; `q = 2*Ichg - 1` | 99.36% agreement with the sign of the truth PDG code over 35 711 truth-matched tracks |
| Lengths (`D0`, `Z0`, vertices) are in **cm** | `Z0` range is exactly ±25, the OPAL selection cut; EDM4hep needs mm, so ×10 |
| `dE/dx` is in **keV/cm** | median 7.70 over tracks with >20 samples, the jet-chamber MIP value |
| `Iluch`, `Iluc`, `Ilucp`, `Iferid` are **PDG codes** | reconstructed masses of `Ptrkh` match PDG values to 5 decimals for all 24 observed species |
| Array indices (`Imtcls`, `Imtkil`, `Imttrk`) are **1-based** | minimum observed value is 1, maximum equals the corresponding counter |
| `Ptrkh`/`Ptrkp`/`Primf` are `(px, py, pz, E)` | invariant mass per species matches PDG (see above) |
| `Pclus`/`Ptrk` are `(px, py, pz)`, cluster energy is `|p|` | massless-cluster convention; consistent with the MT scale factors |
| `Ntrk2 == Ntrk` always | so `Iluc[i]`/`Istrt[i]` describe reconstructed track `i` |

## The MT package

`DACLUS` carries OPAL's double-counting correction. `Imtkil` lists clusters
removed because a matched track already accounts for their energy; `Imtcls`
with `Mtscfc` scales the survivors; `Imttrk`/`Mtscft` do the same for tracks
(`Nmttrk` is 0 in every file inspected, i.e. tracks are not rescaled).

`ReconstructedParticles` is therefore built as **all tracks + all clusters that
are not in `Imtkil`**, with scale factors applied. The validator asserts this
count identity on every event.

## Open question: hadron-level energy exceeds sqrt(s)

In the MC files the hadron-level block sums to more energy than the collision
provides:

    hadron-level sum(E) median 264 GeV   vs   sqrt(s) = 207 GeV   (ratio 1.28)

Individual entries are internally consistent -- every species' reconstructed
mass matches its PDG value, so these are real 4-vectors, not misaligned
buffers -- yet single particles occasionally carry hundreds of GeV, and the
momentum sum does not balance. By contrast the **primary-fermion block is
correct**: `sum(Primf.E)` has median 206.2 GeV, matching `2*Ebeam = 207.0` GeV.

We could not resolve this from the binaries alone; it needs someone with OPAL
knowledge or the QQNT200 documentation. The converter therefore carries the
block through **faithfully and without correction**, and `scripts/validate.py`
reports the ratio on every file so the effect stays visible rather than being
silently laundered into EDM4hep.

Consequences for users: treat `MCParticles` with `generatorStatus == 1`
(hadron level) as suspect until this is understood. The `generatorStatus == 3`
primary fermions are sound.

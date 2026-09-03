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

## Dataset composition

Measured from `Ebeam` across all 359 files (8 252 339 events):

| sqrt(s) | files | events | MC | data | era |
| --- | --- | --- | --- | --- | --- |
| 91 GeV | 42 | 1 333 349 | 34 | 8 | **LEP1** (Z peak) |
| 130-136 GeV | 17 | 264 572 | 13 | 4 | LEP2 |
| 161-172 GeV | 26 | 520 076 | 24 | 2 | LEP2 |
| 183 GeV | 25 | 548 796 | 24 | 1 | LEP2 |
| 189 GeV | 58 | 1 330 420 | 57 | 1 | LEP2 |
| 192-202 GeV | 121 | 2 668 342 | 119 | 2 | LEP2 |
| 205-207 GeV | 70 | 1 586 784 | 70 | 0 | LEP2 |

So the set spans **both eras**: 16.2% of events are LEP1, 83.8% LEP2.

Note that `da91_96` ... `da91_2k_2` and `da1999`/`da2000` are named by *running
year*, not by energy: `da1999_200` averages 191.7 GeV over that year's scan.
Bin by the per-event `Ebeam` (carried in `EventFloats`), never by filename.

## The hadron-level energy anomaly is confined to LEP2 four-fermion samples

The MC hadron-level block sums to more energy than the collision provides --
but only in part of the dataset. Comparing the two eras directly:

| sample | Ievtyp | hadron sum(E) / sqrt(s) | `Primf` sum(E) / sqrt(s) |
| --- | --- | --- | --- |
| LEP1 Z peak (`mc12040_*`) | 0 | **1.000** | not filled |
| LEP2 (`mc10781_*`) | 4 | 1.275 | 0.999 |
| LEP2 (`mc10781_*`) | 7 | 1.159 | 0.999 |

**At the Z peak the block is exact.** The 1.33 M-event LEP1 truth is sound and
needs no caveat. The excess appears only in the LEP2 four-fermion samples
(`Ievtyp` 4 and 7), where it affects ~95% of events.

Within those events the excess is driven by individual *leptonic* entries
carrying more than the beam energy -- one event has a `pdg=-11` entry at
175.9 GeV against `Ebeam` = 103.5 GeV. Dropping every entry above `Ebeam`
overshoots in the other direction (ratio 0.45), so the surplus is not simply a
set of spurious extra particles to be filtered: the individual 4-vectors are
self-consistent (every species' reconstructed mass matches PDG to 5 decimals),
which rules out a misaligned buffer.

We could not resolve the cause from the binaries alone; it needs someone with
OPAL knowledge or the QQNT200 documentation. The converter therefore carries
the block through **faithfully and without correction**, and
`scripts/validate.py` reports the ratio on every file so the effect stays
visible rather than being silently laundered into EDM4hep.

Consequences for users: `MCParticles` with `generatorStatus == 1` (hadron
level) are trustworthy at LEP1 and suspect in the LEP2 four-fermion samples.
The `generatorStatus == 3` primary fermions are sound throughout (ratio 0.999).

# opal-edm4hep

Converts the OPAL QCD ntuples at `/eos/experiment/opal/ntuple/qcd/` into two
deliverables:

1. **a plain ROOT tree** (`<stem>.root`) -- the `h10` TTree exactly as `h2root`
   produces it from the HBOOK file, plus the ~115 bundled histograms;
2. **an EDM4hep file** (`<stem>.edm4hep.root`) -- podio collections built from
   that tree.

The EDM4hep stage reads the plain ROOT tree, never the `.histo` directly, so
stage 1 is a prerequisite and is independently useful.

Modelled on the [DELPHI SDST/FDST converter](https://github.com/DickyChant/delphi-edm4hep),
but the problem is different: OPAL's inputs are already-reduced ntuples, so
there is no Fortran/PHDST dependency and the conversion is a schema mapping
rather than a DST reconstruction.

## Build

```sh
source scripts/setup_env.sh key4hep
cmake -S opal_edm4hep -B build
cmake --build build -j2
ctest --test-dir build
```

## Run

```sh
# one file, both deliverables
scripts/convert_file.sh /eos/experiment/opal/ntuple/qcd/da130_95_200.histo out/

# the whole dataset, staged -- ntuples first, EDM4hep afterwards
scripts/convert_dataset.sh /eos/experiment/opal/ntuple/qcd out/ --stage hbook
scripts/convert_dataset.sh /eos/experiment/opal/ntuple/qcd out/ --stage edm4hep

# check a converted pair
source scripts/setup_env.sh key4hep
python3 scripts/validate.py out/da130_95_200.root out/da130_95_200.edm4hep.root
```

`convert_dataset.sh` is serial and resumable: it skips files whose output
already exists and aborts before it can fill the disk (`--min-free`, default
5 GiB). `--drop-plain` deletes each intermediate tree once its EDM4hep file is
written, for when both do not fit.

## Collections

| Collection | Type | Source |
| --- | --- | --- |
| `EventHeader` | EventHeader | `Irun`, `Ievnt` |
| `ChargedTracks` | Track | `DACTRK`, perigee state recomputed from p, q, `D0`, `Z0` |
| `ChargedTracksDqDx` | RecDqdx | `Dedx`, `Dded` (keV/cm) |
| `Clusters` | Cluster | `DACLUS`, energy `\|p\|`, direction in `iTheta`/`iPhi` |
| `ReconstructedParticles` | ReconstructedParticle | MT package: all tracks + surviving scaled clusters |
| `PrimaryVertex` / `SecondaryVertices` | Vertex | `BTVAR` |
| `MCParticles` | MCParticle | MC only: primary fermions (status 3), partons (2), hadrons (1) |
| `EventFloats` / `EventInts` (+ `*Arrays`) | UserDataCollection | every remaining ntuple variable, columnar |

`ChargedParticles`/`NeutralParticles` are available under `--split-particles`;
they are redundant with `ReconstructedParticles` and cost ~10% in size.

### Event-level variables

EDM4hep models no event shapes, so the DAEVSH/DAXTRA variables travel as
`UserDataCollection` columns with their names written **once** into the
`metadata` frame -- not as podio `GenericParameters`, which would repeat every
key string in every event.

```python
from podio.reading import get_reader
r = get_reader("out/mc10781_1_200.edm4hep.root")
names = [n.decode() for n in r.get("metadata")[0].get_parameter("EventFloatNames")]
values = list(r.get("events")[0].get("EventFloats"))
print(dict(zip(names, values))["Ebeam"])
```

## Jets

Jets are **out of scope for now**. The DAJETS block (jet multiplicities and the
Durham/E0/Cambridge y-resolution scales) is excluded by default: it is the
single largest event-level payload, 30 MB of a 167 MB file. Pass `--jets` to
carry it through; no collection is built from it either way.

## Size and where it can live

Measured on a 95 MB MC file (`mc10781_1_200`):

| Output | Size | vs input |
| --- | --- | --- |
| plain ROOT (`h2root`, zlib:1) | 81 MB | 0.85 |
| EDM4hep, TTree, all variables | 161 MB | 1.69 |
| EDM4hep, TTree, no jets | 104 MB | 1.09 |
| EDM4hep, TTree, no jets, ZSTD:9 | 74 MB | 0.78 |
| **EDM4hep, RNTuple, no jets** | **61 MB** | **0.64** |

RNTuple is the default: it beats a ZSTD:9 recompression pass while writing in
one pass and using less memory. Recompressing the *plain* tree is not worth it
-- the payload is float-dominated, and LZMA:9 buys only 5%.

Projected over the full dataset (31.7 GiB: 18 data files, 341 MC):

| | Size |
| --- | --- |
| plain ROOT | ~27 GiB |
| EDM4hep (RNTuple, no jets) | ~20 GiB |
| both | **~47 GiB** |

Against 42 GiB free on `/mnt/vdb`, **both do not fit**; either alone does.
Use `--drop-plain`, stage the two passes to different filesystems, or free
~6 GiB first.

## What else is on EOS

The QCD ntuples are one directory out of 83.2 TB of OPAL data.
[`docs/data-inventory.md`](docs/data-inventory.md) maps the rest and what
converting it would cost. The short version: `ntuple/gg` (88.8 GB, two-photon
four-fermion samples) already passes stage 1 unchanged and needs only a new
stage-2 mapping, whereas the DST tiers (`ddst`/`csdst`, 5.3 TB real data;
`simd`, 62 TB simulation) need OPAL's own reconstruction software and are a
much larger undertaking.

## Does it work?

[`docs/validation-thrust.md`](docs/validation-thrust.md) plots thrust at the Z
pole from the converted trees -- 425 491 data and 887 486 MC events -- against
detector- and hadron-level Monte Carlo. The shape is textbook, which is the
cheapest end-to-end evidence that the units, indices and normalisation survived
the mapping.

![Thrust at the Z pole](docs/thrust-zpole.png)

## Caveats

- The hadron-level truth block sums to more energy than `sqrt(s)`. It is
  carried through unmodified and flagged by the validator; see
  [`docs/ntuple-schema.md`](docs/ntuple-schema.md).
- The ntuple stores no track covariance, no fit chi2/ndf, no cluster centroid
  (only a direction), and no MCParticle parent/daughter links. Those fields are
  left unset rather than guessed.
- Reco-to-truth links are **not** emitted: the ntuple records only the matched
  PDG code per track (`Iluc`), not an index into the hadron-level list, so no
  faithful `RecoMCParticleLink` can be built. `Iluc`/`Istrt` ride along as
  event columns, ordered to match `ChargedTracks`.

## License

Not yet chosen; until one is added the default "all rights reserved" applies.

# What OPAL data exists on EOS

A map of `/eos/experiment/opal`, measured 2026-09-04 with
`scripts/survey_opal_eos.sh`. This converter currently covers **one directory
out of 83.2 TB**, so the point of this page is to say what the rest is and what
converting it would actually cost.

Sizes come from the `eos.tsize` extended attribute, which EOS maintains per
directory. Never run `du` here: it would walk 83 TB.

## The whole tree

| area | size | share | largest sub-directories |
| --- | --- | --- | --- |
| `simd` | 62.2 TB | 74.7% | `ddst` 55.7 TB, `csdst` 6.0 TB, `cmdst` 442.8 GB, `fatmen` 12.5 GB (+1 more listed) |
| `rawd` | 6.4 TB | 7.6% | `p127` 186.1 GB, `p125` 150.8 GB, `p128` 145.1 GB, `p124` 141.3 GB (+36 more listed) |
| `tape` | 5.3 TB | 6.4% | `R01176` 24.8 GB, `Y00156` 9.7 GB, `Y00055` 9.4 GB, `Y00051` 6.1 GB (+36 more listed) |
| `ddst` | 4.3 TB | 5.2% | `pass7` 2.4 TB, `pass6` 1.4 TB, `pass4` 559.1 GB, `pass2` 29.1 GB (+3 more listed) |
| `tape2` | 2.0 TB | 2.4% | `R01126` 1.9 GB, `R01123` 984.4 MB, `R01132` 93.1 MB, `R01103` 25.2 MB |
| `evki` | 1.2 TB | 1.5% | `ar411mhpy` 38.0 GB, `hw62mh` 36.0 GB, `ar411cr2` 19.0 GB, `ar189ww` 16.8 GB (+36 more listed) |
| `csdst` | 1.0 TB | 1.3% | `pass7` 628.4 GB, `pass6` 279.6 GB, `pass4` 95.1 GB, `pass5` 57.5 GB (+2 more listed) |
| `evbcd` | 460.0 GB | 0.5% | `e1894fac` 21.4 GB, `e183wwac` 13.3 GB, `e1834fac` 9.9 GB, `e1894f` 5.0 GB (+36 more listed) |
| `recover` | 213.0 GB | 0.2% | `P00041` 10.5 GB, `P00034` 4.2 GB, `P00038` 2.7 GB, `P00043` 1.1 GB (+6 more listed) |
| `ntuple` | 120.9 GB | 0.1% | `gg` 88.8 GB, `qcd` 31.7 GB, `fatback` 468.6 MB |
| `pit` | 21.8 GB | 0.0% | `cmdst` 20.9 GB, `u_ws` 920.0 MB |
| `hpopal` | 3.7 GB | 0.0% | `hpopv4` 3.7 GB |
| `archive` | 0 BB | 0.0% | - |
| `test` | 0 BB | 0.0% | - |
Sub-directory listings are capped at 40 entries per area, so for `rawd`, `tape`,
`evki` and `evbcd` the children shown do not sum to the area total. The area
totals themselves are exact.

## Three tiers, very different effort

### 1. Ntuples -- 121 GB, converts with the pipeline in this repo

| | size | status |
| --- | --- | --- |
| `ntuple/qcd` | 31.7 GB | **done** -- 359 files, 8 252 339 events |
| `ntuple/gg` | 88.8 GB | stage 1 works today; stage 2 needs a new mapping |
| `ntuple/fatback` | 469 MB | not looked at |

`ntuple/gg` holds two-photon four-fermion samples (`4f-eeqq`, `4f-eeee`,
`pho_dt`) as `.nt` files with `.log` companions. They are HBOOK column-wise
ntuples like the QCD set, and **`h2root` converts them unchanged** -- verified
on `pho_dt_1986_v102.nt`. Two differences matter for stage 2:

- each file carries **two** ntuples, `h100` (118 branches) and `h101`
  (3 branches: `selrun`, `selevt`, `selwrd`, i.e. a selection index), where the
  QCD files carry one (`h10`);
- the schema is unrelated -- lowercase names (`expn`, `run`, `evt`, `weight`,
  `iugg`, `twop`, `enbeam`, `ntrsel`, `nmcpa`, `mcpx`, ...) rather than the QCD
  blocks. Nothing in `Converter.cpp` transfers; `NtupleReader` and
  `ScalarPassthrough` do.

### 2. Real-data DSTs -- 5.3 TB, the actual preservation target

`ddst` (4.3 TB) and `csdst` (1.0 TB), both with `pass7` as the newest
reprocessing (`ddst/pass7` 2.4 TB, `csdst/pass7` 628 GB). This is the OPAL
analogue of what [`delphi-edm4hep`](https://github.com/DickyChant/delphi-edm4hep)
does for DELPHI SDST/FDST -- and like DELPHI it needs the experiment's own
reconstruction and analysis software (ROPE) to read the banks. `h2root` is no
help here. Substantially bigger than anything in this repo.

### 3. Simulation -- 62 TB

`simd`, three quarters of everything on its own, dominated by `simd/ddst`
(55.7 TB) with `simd/csdst` (6.0 TB) and `simd/cmdst` (443 GB). Same format
problem as tier 2 at 13x the volume.

## The rest

- `rawd` (6.4 TB) -- raw data, by running period (`p124` ... `p128`, 40+).
- `tape` (5.3 TB) / `tape2` (2.0 TB) -- tape-staging areas, by volume id.
- `evki` (1.2 TB) -- generator-level samples; the names identify the generator
  and condition (`ar411mhpy`, `ar411cr2` ARIADNE; `hw62mh` HERWIG; `ar189ww` WW
  at 189 GeV; `babamc` Bhabhas).
- `evbcd` (460 GB) -- four-fermion and WW samples by energy (`e1894fac`,
  `e183wwac`, `e1834fac`).
- `recover` (213 GB), `pit` (21.8 GB, mostly `cmdst`), `hpopal` (3.7 GB).
- `archive` and `test` are empty.

## Reproducing

```sh
scripts/survey_opal_eos.sh > survey.txt      # a few minutes; xattr reads only
```

Each line is `TOP<TAB>area<TAB>bytes` or `SUB<TAB>area/child<TAB>bytes`.

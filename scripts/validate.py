#!/usr/bin/env python3
"""Cross-checks a converted EDM4hep file against its plain-ROOT source.

Run under the key4hep environment:
    source scripts/setup_env.sh key4hep
    python3 scripts/validate.py <plain.root> <converted.edm4hep.root>

Checks that per-event object counts agree with the ntuple counters, that the
unit and charge conventions survived the conversion, and reports the
hadron-level energy-sum anomaly described in docs/ntuple-schema.md.
"""
import sys

import uproot
from podio.reading import get_reader


def main(plain_path, edm_path, n_events=500):
    tree = uproot.open(plain_path)["h10"]
    n = min(n_events, tree.num_entries)
    want = ["Ntrk", "Nclus", "Nmtkil", "Nvtxbt", "Ebeam", "Ichg", "Ptrk", "D0", "Z0"]
    is_mc = "Ntrkh" in tree.keys()
    if is_mc:
        want += ["Ntrkh", "Ptrkh", "Nprimf", "Ntrkp"]
    src = tree.arrays(want, entry_stop=n, library="np")

    events = get_reader(edm_path).get("events")
    failures = []

    def check(cond, msg):
        if not cond:
            failures.append(msg)

    hadron_sums, roots_s = [], []
    for i in range(n):
        f = events[i]
        ntrk = int(src["Ntrk"][i])
        nclus = int(src["Nclus"][i])
        check(len(f.get("ChargedTracks")) == ntrk, f"ev{i}: track count")
        check(len(f.get("Clusters")) == nclus, f"ev{i}: cluster count")
        # MT list = every track plus the clusters the MT package did not kill.
        check(len(f.get("ReconstructedParticles")) == ntrk + nclus - int(src["Nmtkil"][i]),
              f"ev{i}: reco particle count")
        check(len(f.get("SecondaryVertices")) == int(src["Nvtxbt"][i]), f"ev{i}: vertex count")

        if ntrk:
            ts = f.get("ChargedTracks")[0].getTrackStates()[0]
            # cm -> mm
            check(abs(ts.D0 - src["D0"][i][0] * 10.0) < 1e-3, f"ev{i}: D0 unit")
            check(abs(ts.Z0 - src["Z0"][i][0] * 10.0) < 1e-3, f"ev{i}: Z0 unit")
            q = f.get("ReconstructedParticles")[0].getCharge()
            check(abs(q - (2 * int(src["Ichg"][i][0]) - 1)) < 1e-6, f"ev{i}: charge")

        if is_mc:
            nmc = int(src["Nprimf"][i]) + int(src["Ntrkp"][i]) + int(src["Ntrkh"][i])
            got = len(f.get("MCParticles"))
            check(got in (nmc, nmc + 1), f"ev{i}: MC count {got} vs {nmc}(+ISR)")
            P = src["Ptrkh"][i]
            if len(P):
                hadron_sums.append(float(P[:, 3].sum()))
                roots_s.append(2.0 * float(src["Ebeam"][i]))

    print(f"{'MC' if is_mc else 'data'}: checked {n} events")
    if failures:
        for m in failures[:10]:
            print("  FAIL", m)
        print(f"  {len(failures)} failure(s)")
    else:
        print("  all structural and unit checks passed")

    if hadron_sums:
        import statistics
        med = statistics.median(hadron_sums)
        rs = statistics.median(roots_s)
        print(f"  hadron-level sum(E) median {med:.1f} GeV vs sqrt(s) {rs:.1f} GeV "
              f"(ratio {med / rs:.2f})")
        if med > 1.05 * rs:
            print("  NOTE: hadron-level energy exceeds sqrt(s); this is a property of the\n"
                  "        source ntuple, carried through faithfully. See docs/ntuple-schema.md.")
    return 1 if failures else 0


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1], sys.argv[2]))

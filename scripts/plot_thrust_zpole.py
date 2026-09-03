#!/usr/bin/env python3
"""Thrust distribution at the Z pole from the converted OPAL ntuples.

Plots 1/N dN/d(1-T) for OPAL Z-peak data against the corresponding Monte
Carlo, at both detector and hadron level. The ntuple stores 1-T (not T).

    python3 scripts/plot_thrust_zpole.py <dir-of-plain-root-files> [out.png]
"""
import glob
import os
import sys

import numpy as np
import uproot

# --- design tokens (validated categorical slots 1-3, light surface) ---------
SURFACE = "#fcfcfb"
INK, INK_2, INK_3 = "#0b0b0b", "#52514e", "#8a8880"
DATA_C, RECO_C, HAD_C = "#2a78d6", "#eb6834", "#1baf7a"

BINS = np.linspace(0.0, 0.45, 91)          # 1-T
CENTRES = 0.5 * (BINS[1:] + BINS[:-1])
WIDTHS = np.diff(BINS)
MIN_TRACKS = 5                              # multihadron selection


def accumulate(files, branches, ntrk_cut=MIN_TRACKS):
    """Histogram `branches` over `files`, reading in bounded-memory chunks."""
    out = {b: np.zeros(len(BINS) - 1) for b in branches}
    n_sel = 0
    for path in files:
        try:
            tree = uproot.open(path)["h10"]
        except Exception:
            continue
        have = [b for b in branches if b in tree.keys()]
        if not have:
            continue
        for chunk in tree.iterate(have + ["Ntrk"], step_size=20000, library="np"):
            keep = chunk["Ntrk"] >= ntrk_cut
            n_sel += int(keep.sum())
            for b in have:
                v = chunk[b][keep]
                out[b] += np.histogram(v[v > 0], bins=BINS)[0]
    return out, n_sel


def normalise(counts):
    """1/N dN/d(1-T) with Poisson errors."""
    total = counts.sum()
    if total == 0:
        return counts, counts
    return counts / (total * WIDTHS), np.sqrt(counts) / (total * WIDTHS)


def load_csv(path):
    """Replot from a previous run's table without re-reading 42 ROOT files."""
    n_dat = n_mc = -1
    with open(path) as fh:
        head = fh.readline()
    if head.startswith("#"):
        kv = dict(p.split("=") for p in head.lstrip("# ").strip().split(","))
        n_dat, n_mc = int(kv["n_data"]), int(kv["n_mc"])
    # genfromtxt would otherwise read the '#' provenance line as the header
    rows = np.genfromtxt(path, delimiter=",", names=True,
                         skip_header=1 if head.startswith("#") else 0)
    return (rows["data"], rows["data_err"], rows["mc_detector"], rows["mc_hadron"],
            n_dat, n_mc)


def main(indir, outpng="thrust_zpole.png"):
    csv = os.path.splitext(outpng)[0] + ".csv"
    if os.path.exists(csv) and os.environ.get("REPLOT"):
        y_dat, e_dat, y_reco, y_had, n_dat, n_mc = load_csv(csv)
        return render(y_dat, e_dat, y_reco, y_had, n_dat, n_mc, outpng, csv)

    all_files = sorted(glob.glob(os.path.join(indir, "*.root")))
    data_files, mc_files = [], []
    for f in all_files:
        try:
            t = uproot.open(f)["h10"]
            if float(t["Ebeam"].array(entry_stop=1, library="np")[0]) > 47.5:
                continue                     # LEP2
            (mc_files if "Ntrkh" in t.keys() else data_files).append(f)
        except Exception:
            continue
    print(f"Z pole: {len(data_files)} data files, {len(mc_files)} MC files")

    dat, n_dat = accumulate(data_files, ["Tdtc"])
    mc, n_mc = accumulate(mc_files, ["Tdtc", "Th"])
    print(f"selected: {n_dat} data events, {n_mc} MC events")

    y_dat, e_dat = normalise(dat["Tdtc"])
    y_reco, _ = normalise(mc["Tdtc"])
    y_had, _ = normalise(mc["Th"])
    return render(y_dat, e_dat, y_reco, y_had, n_dat, n_mc, outpng,
                  os.path.splitext(outpng)[0] + ".csv")


def render(y_dat, e_dat, y_reco, y_had, n_dat, n_mc, outpng, csv):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.gridspec import GridSpec

    fig = plt.figure(figsize=(8.2, 7.4), dpi=200, facecolor=SURFACE)
    gs = GridSpec(2, 1, height_ratios=[3.1, 1], hspace=0.07)
    ax = fig.add_subplot(gs[0], facecolor=SURFACE)
    rx = fig.add_subplot(gs[1], facecolor=SURFACE, sharex=ax)

    for a in (ax, rx):
        a.grid(True, which="major", color=INK_3, alpha=0.18, lw=0.7)
        for s in ("top", "right"):
            a.spines[s].set_visible(False)
        for s in ("left", "bottom"):
            a.spines[s].set_color(INK_3)
            a.spines[s].set_linewidth(0.8)
        a.tick_params(colors=INK_2, labelsize=10, length=3, width=0.8)

    ok = y_dat > 0
    ax.step(CENTRES, y_had, where="mid", color=HAD_C, lw=2.0,
            label="MC, hadron level (truth)")
    ax.step(CENTRES, y_reco, where="mid", color=RECO_C, lw=2.0,
            label="MC, detector level")
    ax.errorbar(CENTRES[ok], y_dat[ok], yerr=e_dat[ok], fmt="o", ms=4,
                color=DATA_C, mfc=SURFACE, mew=1.4, lw=1.2, capsize=0,
                label="OPAL data")

    ax.set_yscale("log")
    ax.set_ylabel(r"$\frac{1}{N}\,\frac{dN}{d(1-T)}$", color=INK, fontsize=14)
    ax.set_xlim(0, 0.45)
    ax.set_ylim(2e-3, 60)
    ax.set_title("Thrust at the Z pole", color=INK, fontsize=15,
                 fontweight="600", loc="left", pad=14)
    counts = ("" if n_dat < 0 else
              f"  ·  {n_dat:,} data events  ·  {n_mc:,} MC events")
    ax.text(0, 1.012, f"OPAL, $\\sqrt{{s}}\\approx$ 91 GeV{counts}",
            transform=ax.transAxes, color=INK_2, fontsize=10)

    # Direct labels: required relief for the aqua contrast warning, and they
    # let the reader identify a curve without crossing to the legend.
    lead = dict(textcoords="offset points", fontsize=11, fontweight="600")
    ax.annotate("OPAL data", (CENTRES[14], y_dat[14]), xytext=(22, 26),
                color=DATA_C, arrowprops=dict(arrowstyle="-", color=DATA_C,
                lw=1.0, shrinkA=2, shrinkB=3), **lead)
    ax.annotate("MC, detector level", (CENTRES[40], y_reco[40]), xytext=(30, 30),
                color=RECO_C, arrowprops=dict(arrowstyle="-", color=RECO_C,
                lw=1.0, shrinkA=2, shrinkB=3), **lead)
    ax.annotate("MC, hadron level", (CENTRES[56], y_had[56]), xytext=(-150, -46),
                color=HAD_C, arrowprops=dict(arrowstyle="-", color=HAD_C,
                lw=1.0, shrinkA=2, shrinkB=3), **lead)
    plt.setp(ax.get_xticklabels(), visible=False)

    with np.errstate(divide="ignore", invalid="ignore"):
        ratio = np.where(y_reco > 0, y_dat / y_reco, np.nan)
        rerr = np.where(y_reco > 0, e_dat / y_reco, np.nan)
    rx.axhline(1.0, color=RECO_C, lw=2.0)
    rx.errorbar(CENTRES[ok], ratio[ok], yerr=rerr[ok], fmt="o", ms=4,
                color=DATA_C, mfc=SURFACE, mew=1.4, lw=1.2, capsize=0)
    rx.set_ylim(0.5, 1.5)
    rx.set_ylabel("data / MC", color=INK, fontsize=11)
    rx.set_xlabel("$1-T$", color=INK, fontsize=13)

    fig.savefig(outpng, dpi=200, bbox_inches="tight", facecolor=SURFACE)
    print("wrote", outpng)

    # Table view: the relief rule's second option, and useful on its own.
    csv = os.path.splitext(outpng)[0] + ".csv"
    with open(csv, "w") as fh:
        fh.write(f"# n_data={n_dat},n_mc={n_mc}\n")
        fh.write("bin_low,bin_high,data,data_err,mc_detector,mc_hadron,ratio\n")
        for i in range(len(CENTRES)):
            fh.write(f"{BINS[i]:.4f},{BINS[i+1]:.4f},{y_dat[i]:.6g},{e_dat[i]:.6g},"
                     f"{y_reco[i]:.6g},{y_had[i]:.6g},{ratio[i]:.6g}\n")
    print("wrote", csv)


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "/mnt/vdb/opal-data/qcd-root",
         sys.argv[2] if len(sys.argv) > 2 else "thrust_zpole.png")

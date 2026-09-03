/// Converts one h2root-produced OPAL ntuple file into EDM4hep.
///
/// Usage: opal_ntuple_pass [options] <input.root> <output.edm4hep.root>

#include "opal_edm4hep/Converter.h"
#include "opal_edm4hep/NtupleReader.h"
#include "opal_edm4hep/ScalarPassthrough.h"

#include <podio/ROOTWriter.h>
#include <podio/RNTupleWriter.h>
#include <podio/UserDataCollection.h>

#include <TROOT.h>

#include <cstring>
#include <exception>
#include <iostream>
#include <set>
#include <string>

namespace {

/// Branches consumed by the structured conversion. Everything else is carried
/// across verbatim as Frame parameters by ScalarPassthrough.
const std::set<std::string> kStructuredBranches = {
    "Irun",    "Ievnt",   "Ntrk",     "Ptrk",    "Ichg",     "D0",       "Z0",
    "Dp",      "Dedx",    "Dded",     "Nhde",    "Nhcj",     "Id02",     "Nclus",
    "Pclus",   "Nmttrk",  "Imttrk",   "Mtscft",  "Nmtcls",   "Imtcls",   "Mtscfc",
    "Nmtkil",  "Imtkil",  "Nvtxbt",   "Prvtxbt", "Vtxbt",    "Vchi2bt",  "Vnnbt",
    "Ivmulbt", "Vdlen3bt", "Vderr3bt", "Nprimf", "Iferid",   "Primf",    "Pisr",
    "Ntrkp",   "Ilucp",   "Ptrkp",    "Ntrkh",   "Iluch",    "Ichgh",    "Ptrkh"};

/// The DAJETS block: jet multiplicities and the Durham/E0/Cambridge
/// y-resolution scales at every merge step, for each detector-object
/// definition and (MC) each truth level. These variable-length arrays are by
/// far the largest event-level payload -- measured at 30 MB of a 167 MB file --
/// so they are excluded unless --jets is given.
const std::set<std::string> kJetBranches = {
    "Nxjdtc", "Nxjdt",   "Nxjdc",  "Nxjdmt",  "Nxjetc",  "Nxjet",   "Nxjec",
    "Nxjemt", "Nxjctc",  "Nxjct",  "Nxjcc",   "Nxjcmt",  "Yddtc",   "Yedtc",
    "Ycdtc",  "Njcedtc", "Njcrdtc", "Yddt",   "Yedt",    "Ycdt",    "Njcedt",
    "Njcrdt", "Yddc",    "Yedc",   "Ycdc",    "Njcedc",  "Njcrdc",  "Yddmt",
    "Yedmt",  "Ycdmt",   "Njcedmt", "Njcrdmt", "Nxjdp",  "Nxjdh",   "Nxjep",
    "Nxjeh",  "Nxjcp",   "Nxjch",  "Ydp",     "Yep",     "Ycp",     "Njcep",
    "Njcrp",  "Ydh",     "Yeh",    "Ych",     "Njceh",   "Njcrh"};

void usage() {
  std::cerr
      << "usage: opal_ntuple_pass [options] <input.root> <output.edm4hep.root>\n"
      << "  -n <N>    convert at most N events\n"
      << "  -t <tree> input tree name (default: h10)\n"
      << "  -q        suppress the per-file summary\n"
      << "  --jets    also carry the DAJETS jet-resolution arrays (adds ~18% size)\n"
      << "  --rntuple write RNTuple instead of TTree; compresses with ZSTD in a\n"
      << "            single pass, so no recompress step is needed\n"
      << "  --split-particles\n"
      << "            also write ChargedParticles/NeutralParticles, which are\n"
      << "            redundant with ReconstructedParticles (adds ~10% size)\n"
      << "\n"
      << "Output compression is podio's default (zlib:1). Recompression to\n"
      << "ZSTD/LZMA is an explicit pipeline step -- see scripts/recompress.sh --\n"
      << "because podio::ROOTWriter exposes no compression setting and ROOT's\n"
      << "gEnv keys are not honoured for the file it opens internally.\n";
}

} // namespace

int main(int argc, char** argv) {
  std::string input, output, treeName = "h10";
  long long maxEvents = -1;
  bool quiet = false, withJets = false, splitParticles = false, rntuple = false;

  int i = 1;
  for (; i < argc; ++i) {
    if (std::strcmp(argv[i], "-n") == 0 && i + 1 < argc) maxEvents = std::stoll(argv[++i]);
    else if (std::strcmp(argv[i], "-t") == 0 && i + 1 < argc) treeName = argv[++i];
    else if (std::strcmp(argv[i], "-q") == 0) quiet = true;
    else if (std::strcmp(argv[i], "--jets") == 0) withJets = true;
    else if (std::strcmp(argv[i], "--rntuple") == 0) rntuple = true;
    else if (std::strcmp(argv[i], "--split-particles") == 0) splitParticles = true;
    else if (std::strcmp(argv[i], "-h") == 0) { usage(); return 0; }
    else break;
  }
  if (argc - i != 2) { usage(); return 1; }
  input = argv[i];
  output = argv[i + 1];

  try {
    // The conversion is strictly single-threaded and event-at-a-time: the
    // dataset is ~32 GB and the conversion hosts are memory-constrained.
    gROOT->SetBatch(true);

    opal::NtupleReader reader(input, treeName);
    auto skip = kStructuredBranches;
    if (!withJets) skip.insert(kJetBranches.begin(), kJetBranches.end());
    opal::ScalarPassthrough extras(reader.reader(), reader.tree(), skip);

    opal::ConversionStats stats;
    opal::ScalarPassthrough::Packed packed;

    auto run = [&](auto& writer) {
    while (reader.next()) {
      if (maxEvents >= 0 && static_cast<long long>(stats.events) >= maxEvents) break;
      auto frame = opal::convertEvent(reader, stats, splitParticles);
      extras.refresh(packed);

      // Columnar carry-over of the ntuple variables EDM4hep does not model.
      // The names live in the metadata frame, written once below.
      podio::UserDataCollection<float> floats;
      floats.vec() = packed.floats;
      podio::UserDataCollection<std::int32_t> ints;
      ints.vec() = packed.ints;
      podio::UserDataCollection<float> arrFloats;
      arrFloats.vec() = packed.floatArrayData;
      podio::UserDataCollection<std::uint32_t> arrFloatEnds;
      arrFloatEnds.vec() = packed.floatArrayEnds;
      podio::UserDataCollection<std::int32_t> arrInts;
      arrInts.vec() = packed.intArrayData;
      podio::UserDataCollection<std::uint32_t> arrIntEnds;
      arrIntEnds.vec() = packed.intArrayEnds;

      frame.put(std::move(floats), "EventFloats");
      frame.put(std::move(ints), "EventInts");
      frame.put(std::move(arrFloats), "EventFloatArrays");
      frame.put(std::move(arrFloatEnds), "EventFloatArrayEnds");
      frame.put(std::move(arrInts), "EventIntArrays");
      frame.put(std::move(arrIntEnds), "EventIntArrayEnds");

      writer.writeFrame(frame, "events");
    }

    // One metadata frame naming the columns, so the values above are decodable.
    podio::Frame metadata;
    metadata.putParameter("EventFloatNames", extras.floatNames());
    metadata.putParameter("EventIntNames", extras.intNames());
    metadata.putParameter("EventFloatArrayNames", extras.floatArrayNames());
    metadata.putParameter("EventIntArrayNames", extras.intArrayNames());
    metadata.putParameter("OpalSource", reader.isMonteCarlo() ? std::string("MC") : std::string("data"));
    metadata.putParameter("OpalInputFile", input);
    writer.writeFrame(metadata, "metadata");
    writer.finish();
    };

    if (rntuple) {
      podio::RNTupleWriter writer(output);
      run(writer);
    } else {
      podio::ROOTWriter writer(output);
      run(writer);
    }

    if (!quiet) {
      std::cout << "converted " << stats.events << " events from " << input << "\n"
                << "  source        : " << (reader.isMonteCarlo() ? "MC" : "data") << "\n"
                << "  passthrough   : " << extras.boundCount() << " variables"
                << (withJets ? " (incl. DAJETS)" : " (DAJETS excluded)") << "\n"
                << "  tracks        : " << stats.tracks << "\n"
                << "  clusters      : " << stats.clusters << "\n"
                << "  reco particles: " << stats.recoParticles << "\n"
                << "  vertices      : " << stats.vertices << "\n"
                << "  MC particles  : " << stats.mcParticles << "\n"
                << "  output        : " << output << "\n";
    }
  } catch (const std::exception& e) {
    std::cerr << "opal_ntuple_pass: " << e.what() << "\n";
    return 1;
  }
  return 0;
}

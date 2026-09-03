#include "opal_edm4hep/Converter.h"
#include "opal_edm4hep/Helix.h"
#include "opal_edm4hep/OpalUnits.h"

#include <edm4hep/ClusterCollection.h>
#include <edm4hep/EventHeaderCollection.h>
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/RecDqdxCollection.h>
#include <edm4hep/ReconstructedParticleCollection.h>
#include <edm4hep/TrackCollection.h>
#include <edm4hep/VertexCollection.h>

#include <cmath>
#include <vector>

namespace opal {
namespace {

/// Charged tracks carry no mass hypothesis in the ntuple; OPAL analyses treat
/// them as pions, which is also what the MT package assumes when it balances
/// track and cluster energy.
constexpr double kPionMass = 0.13957;

/// Generator status codes used for the three truth layers the ntuple stores.
constexpr int kStatusFinalState = 1;  ///< hadron level
constexpr int kStatusParton = 2;      ///< parton level (after the shower)
constexpr int kStatusHardProcess = 3; ///< primary fermions and the ISR photon

double magnitude(double x, double y, double z) { return std::sqrt(x * x + y * y + z * z); }

} // namespace

podio::Frame convertEvent(const NtupleReader& r, ConversionStats& stats,
                          bool splitParticles) {
  podio::Frame frame;

  // ---- event header ------------------------------------------------------
  edm4hep::EventHeaderCollection headers;
  auto header = headers.create();
  header.setRunNumber(static_cast<std::uint32_t>(r.irun()));
  header.setEventNumber(static_cast<std::uint64_t>(r.ievnt()));
  header.setWeight(1.0);

  // ---- DACTRK: charged tracks -------------------------------------------
  edm4hep::TrackCollection tracks;
  edm4hep::RecDqdxCollection dqdx;
  edm4hep::ReconstructedParticleCollection charged;

  const int nTrk = r.ntrk();
  for (int i = 0; i < nTrk; ++i) {
    const double px = r.ptrk(i, 0), py = r.ptrk(i, 1), pz = r.ptrk(i, 2);
    const float q = chargeFromIchg(r.ichg(i));

    auto track = tracks.create();
    track.addToTrackStates(makePerigeeState(px, py, pz, q, r.d0(i), r.z0(i)));
    // The ntuple keeps hit counts but no fit chi2/ndf, so those stay unset.
    track.addToSubdetectorHitNumbers(r.nhcj(i)); // jet chamber (CJ)
    track.addToSubdetectorHitNumbers(r.nhde(i)); // dE/dx samples
    track.setType(r.id02(i)); // d02 good-track flag

    auto dq = dqdx.create();
    dq.setTrack(track);
    dq.setDQdx(edm4hep::Quantity{units::kDqDxTypeKeVPerCm, r.dedx(i), r.dded(i)});

    const double p = magnitude(px, py, pz);
    auto particle = charged.create();
    particle.setCharge(q);
    particle.setMass(static_cast<float>(kPionMass));
    particle.setMomentum({static_cast<float>(px), static_cast<float>(py),
                          static_cast<float>(pz)});
    particle.setEnergy(static_cast<float>(std::sqrt(p * p + kPionMass * kPionMass)));
    particle.setPDG(static_cast<int>(q) * 211);
    particle.addToTracks(track);
    // Dp is the relative momentum error; promote it to the momentum block of
    // the 4-momentum covariance so the uncertainty is not lost.
    const double dpAbs = r.dp(i) * p;
    particle.setCovMatrix(static_cast<float>(dpAbs * dpAbs),
                          edm4hep::FourMomCoords::x, edm4hep::FourMomCoords::x);
  }
  stats.tracks += nTrk;

  // ---- DACLUS: calorimeter clusters --------------------------------------
  edm4hep::ClusterCollection clusters;
  edm4hep::ReconstructedParticleCollection neutrals;

  const int nClus = r.nclus();
  for (int i = 0; i < nClus; ++i) {
    const double px = r.pclus(i, 0), py = r.pclus(i, 1), pz = r.pclus(i, 2);
    const double e = magnitude(px, py, pz);

    auto cluster = clusters.create();
    cluster.setEnergy(static_cast<float>(e));
    // The ntuple stores only the cluster direction, not its centroid, so the
    // direction goes into iTheta/iPhi and position is deliberately left unset.
    cluster.setITheta(e > 0.0 ? static_cast<float>(std::acos(pz / e)) : 0.0f);
    cluster.setIPhi(static_cast<float>(std::atan2(py, px)));

    auto neutral = neutrals.create();
    neutral.setCharge(0.0f);
    neutral.setMass(0.0f);
    neutral.setEnergy(static_cast<float>(e));
    neutral.setMomentum({static_cast<float>(px), static_cast<float>(py),
                         static_cast<float>(pz)});
    neutral.addToClusters(cluster);
  }
  stats.clusters += nClus;

  // ---- MT package: the double-counting-corrected particle list -----------
  // Imtkil lists clusters removed because their energy is already carried by a
  // matched track; Imtcls/Mtscfc scale the survivors. Indices are 1-based.
  std::vector<float> clusterScale(nClus, 1.0f);
  std::vector<bool> clusterKilled(nClus, false);
  for (int i = 0; i < r.nmtkil(); ++i) {
    const int idx = fortranIndexToOffset(r.imtkil(i));
    if (idx >= 0 && idx < nClus) clusterKilled[idx] = true;
  }
  for (int i = 0; i < r.nmtcls(); ++i) {
    const int idx = fortranIndexToOffset(r.imtcls(i));
    if (idx >= 0 && idx < nClus) clusterScale[idx] = r.mtscfc(i);
  }
  std::vector<float> trackScale(nTrk, 1.0f);
  for (int i = 0; i < r.nmttrk(); ++i) {
    const int idx = fortranIndexToOffset(r.imttrk(i));
    if (idx >= 0 && idx < nTrk) trackScale[idx] = r.mtscft(i);
  }

  edm4hep::ReconstructedParticleCollection recoParticles;
  for (int i = 0; i < nTrk; ++i) {
    const double s = trackScale[i];
    const double px = r.ptrk(i, 0) * s, py = r.ptrk(i, 1) * s, pz = r.ptrk(i, 2) * s;
    const double p = magnitude(px, py, pz);
    auto particle = recoParticles.create();
    particle.setCharge(chargeFromIchg(r.ichg(i)));
    particle.setMass(static_cast<float>(kPionMass));
    particle.setMomentum({static_cast<float>(px), static_cast<float>(py),
                          static_cast<float>(pz)});
    particle.setEnergy(static_cast<float>(std::sqrt(p * p + kPionMass * kPionMass)));
    particle.setPDG(static_cast<int>(chargeFromIchg(r.ichg(i))) * 211);
    particle.addToTracks(tracks[i]);
  }
  for (int i = 0; i < nClus; ++i) {
    if (clusterKilled[i]) continue;
    const double s = clusterScale[i];
    const double px = r.pclus(i, 0) * s, py = r.pclus(i, 1) * s, pz = r.pclus(i, 2) * s;
    auto particle = recoParticles.create();
    particle.setCharge(0.0f);
    particle.setMass(0.0f);
    particle.setEnergy(static_cast<float>(magnitude(px, py, pz)));
    particle.setMomentum({static_cast<float>(px), static_cast<float>(py),
                          static_cast<float>(pz)});
    particle.addToClusters(clusters[i]);
  }
  stats.recoParticles += recoParticles.size();

  // ---- BTVAR: primary and secondary vertices -----------------------------
  edm4hep::VertexCollection primaryVertex;
  {
    auto pv = primaryVertex.create();
    pv.setPrimary(true);
    pv.setPosition({static_cast<float>(r.prvtxbt()[0] * units::kCmToMm),
                    static_cast<float>(r.prvtxbt()[1] * units::kCmToMm),
                    static_cast<float>(r.prvtxbt()[2] * units::kCmToMm)});
  }

  edm4hep::VertexCollection secondaryVertices;
  for (int i = 0; i < r.nvtxbt(); ++i) {
    auto sv = secondaryVertices.create();
    sv.setSecondary(true);
    sv.setPosition({static_cast<float>(r.vtxbt(i, 0) * units::kCmToMm),
                    static_cast<float>(r.vtxbt(i, 1) * units::kCmToMm),
                    static_cast<float>(r.vtxbt(i, 2) * units::kCmToMm)});
    sv.setChi2(r.vchi2bt(i));
    // Parameter order is fixed and documented in docs/ntuple-schema.md:
    // [0] b-tag network output, [1] 3D decay length, [2] its error,
    // [3] track multiplicity at the vertex.
    sv.addToParameters(r.vnnbt(i));
    sv.addToParameters(r.vdlen3bt(i));
    sv.addToParameters(r.vderr3bt(i));
    sv.addToParameters(static_cast<float>(r.ivmulbt(i)));
  }
  stats.vertices += 1 + r.nvtxbt();

  // ---- truth blocks ------------------------------------------------------
  edm4hep::MCParticleCollection mcParticles;
  if (r.isMonteCarlo()) {
    // Primary fermions of the hard process.
    for (int i = 0; i < r.nprimf(); ++i) {
      auto mc = mcParticles.create();
      mc.setPDG(r.iferid(i));
      mc.setGeneratorStatus(kStatusHardProcess);
      mc.setMomentum({r.primf(i, 0), r.primf(i, 1), r.primf(i, 2)});
      const double e = r.primf(i, 3);
      const double p = magnitude(r.primf(i, 0), r.primf(i, 1), r.primf(i, 2));
      mc.setMass(e * e > p * p ? std::sqrt(e * e - p * p) : 0.0);
    }
    // ISR photon, when the event has one.
    if (magnitude(r.pisr()[0], r.pisr()[1], r.pisr()[2]) > 0.0) {
      auto mc = mcParticles.create();
      mc.setPDG(22);
      mc.setGeneratorStatus(kStatusHardProcess);
      mc.setMomentum({r.pisr()[0], r.pisr()[1], r.pisr()[2]});
      mc.setMass(0.0);
    }
    // Parton level, after the shower.
    for (int i = 0; i < r.ntrkp(); ++i) {
      auto mc = mcParticles.create();
      mc.setPDG(r.ilucp(i));
      mc.setGeneratorStatus(kStatusParton);
      mc.setMomentum({r.ptrkp(i, 0), r.ptrkp(i, 1), r.ptrkp(i, 2)});
    }
    // Hadron level: the final-state particles.
    for (int i = 0; i < r.ntrkh(); ++i) {
      auto mc = mcParticles.create();
      mc.setPDG(r.iluch(i));
      mc.setGeneratorStatus(kStatusFinalState);
      mc.setCharge(static_cast<float>(r.ichgh(i)));
      mc.setMomentum({r.ptrkh(i, 0), r.ptrkh(i, 1), r.ptrkh(i, 2)});
      const double e = r.ptrkh(i, 3);
      const double p = magnitude(r.ptrkh(i, 0), r.ptrkh(i, 1), r.ptrkh(i, 2));
      mc.setMass(e * e > p * p ? std::sqrt(e * e - p * p) : 0.0);
    }
    // The ntuple records no parent/daughter indices, so the MCParticle tree is
    // deliberately left flat rather than being guessed at.
    stats.mcParticles += mcParticles.size();
  }

  frame.put(std::move(headers), coll::kEventHeader);
  frame.put(std::move(tracks), coll::kTracks);
  frame.put(std::move(dqdx), coll::kDqDx);

  frame.put(std::move(clusters), coll::kClusters);

  frame.put(std::move(recoParticles), coll::kRecoParticles);
  frame.put(std::move(primaryVertex), coll::kPrimaryVertex);
  frame.put(std::move(secondaryVertices), coll::kSecondaryVertices);
  if (splitParticles) {
    frame.put(std::move(charged), coll::kChargedParticles);
    frame.put(std::move(neutrals), coll::kNeutralParticles);
  }
  if (r.isMonteCarlo()) frame.put(std::move(mcParticles), coll::kMCParticles);

  stats.events += 1;
  return frame;
}

} // namespace opal

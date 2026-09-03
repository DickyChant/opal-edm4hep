#ifndef OPAL_EDM4HEP_CONVERTER_H
#define OPAL_EDM4HEP_CONVERTER_H

#include "opal_edm4hep/NtupleReader.h"

#include <podio/Frame.h>

#include <cstdint>
#include <string>

namespace opal {

/// Names of every collection the converter can emit. Kept in one place so the
/// tests and the validator agree with the writer.
namespace coll {
inline constexpr const char* kEventHeader = "EventHeader";
inline constexpr const char* kTracks = "ChargedTracks";
inline constexpr const char* kDqDx = "ChargedTracksDqDx";
inline constexpr const char* kChargedParticles = "ChargedParticles";
inline constexpr const char* kClusters = "Clusters";
inline constexpr const char* kNeutralParticles = "NeutralParticles";
inline constexpr const char* kRecoParticles = "ReconstructedParticles";
inline constexpr const char* kPrimaryVertex = "PrimaryVertex";
inline constexpr const char* kSecondaryVertices = "SecondaryVertices";
inline constexpr const char* kMCParticles = "MCParticles";
} // namespace coll

/// Counters accumulated over a whole file, reported at the end of a run.
struct ConversionStats {
  std::uint64_t events = 0;
  std::uint64_t tracks = 0;
  std::uint64_t clusters = 0;
  std::uint64_t recoParticles = 0;
  std::uint64_t vertices = 0;
  std::uint64_t mcParticles = 0;
};

/// Converts one event, currently positioned in `reader`, into a podio Frame.
///
/// Event-level scalars that EDM4hep has no datatype for -- the DAEVSH event
/// shapes, the DAJETS jet-resolution scales and the DAXTRA selection variables
/// -- are attached as Frame parameters rather than being dropped.
///
/// No jet collection is produced: jets are deliberately out of scope for now.
/// The DAJETS variables still travel across as Frame parameters, so promoting
/// them later needs no re-conversion of the inputs.
/// @param splitParticles also emit ChargedParticles/NeutralParticles. They are
///        redundant with ReconstructedParticles (same objects, before the MT
///        double-counting correction) and are off by default to save space.
podio::Frame convertEvent(const NtupleReader& reader, ConversionStats& stats,
                          bool splitParticles = false);

} // namespace opal

#endif

#ifndef OPAL_EDM4HEP_OPALUNITS_H
#define OPAL_EDM4HEP_OPALUNITS_H

namespace opal {

/// Detector and unit conventions of the OPAL QCD ntuple (`QQNT200`).
///
/// Every constant here was established against the ntuple contents rather than
/// assumed; see `docs/ntuple-schema.md` for the measurements that back each one.
namespace units {

/// OPAL solenoid field, Tesla. Used for the p -> omega conversion only.
inline constexpr double kBFieldTesla = 0.435;

/// Ntuple lengths (D0, Z0, vertex positions) are in centimetres; EDM4hep is mm.
inline constexpr double kCmToMm = 10.0;

/// omega [1/mm] = kOmegaFactor * q * B[T] / pt[GeV].
/// 1/R[mm] = 0.299792458 * B / (1000 * pt).
inline constexpr double kOmegaFactor = 2.99792458e-4;

/// dE/dx in the jet chamber is stored in keV/cm (MIP sits at ~7.7).
inline constexpr int kDqDxTypeKeVPerCm = 0;

} // namespace units

/// Reconstructed charge from the ntuple's `Ichg` flag.
///
/// `Ichg` is stored as 0/1, not as a signed charge. Validated against
/// truth PDG sign on 35711 truth-matched tracks: 99.36% agreement.
inline constexpr float chargeFromIchg(int ichg) {
  return 2.0f * static_cast<float>(ichg) - 1.0f;
}

/// Fortran arrays in the ntuple are 1-based; EDM4hep collections are 0-based.
/// Returns a negative value when the index is the Fortran "unset" 0.
inline constexpr int fortranIndexToOffset(int oneBased) { return oneBased - 1; }

} // namespace opal

#endif

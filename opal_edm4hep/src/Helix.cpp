#include "opal_edm4hep/Helix.h"
#include "opal_edm4hep/OpalUnits.h"

#include <cmath>

namespace opal {

edm4hep::TrackState makePerigeeState(double px, double py, double pz, float charge,
                                     double d0Cm, double z0Cm) {
  edm4hep::TrackState st;
  st.location = edm4hep::TrackState::AtIP;

  const double pt = std::hypot(px, py);
  st.phi = static_cast<float>(std::atan2(py, px));
  st.tanLambda = pt > 0.0 ? static_cast<float>(pz / pt) : 0.0f;

  // omega carries the sign of the charge; zero pt would be unphysical but is
  // guarded so a corrupt entry cannot produce an inf.
  st.omega = pt > 0.0 ? static_cast<float>(charge * units::kOmegaFactor *
                                           units::kBFieldTesla / pt)
                      : 0.0f;

  st.D0 = static_cast<float>(d0Cm * units::kCmToMm);
  st.Z0 = static_cast<float>(z0Cm * units::kCmToMm);
  st.referencePoint = {0.0f, 0.0f, 0.0f};
  return st;
}

} // namespace opal

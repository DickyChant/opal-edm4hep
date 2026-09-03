#ifndef OPAL_EDM4HEP_HELIX_H
#define OPAL_EDM4HEP_HELIX_H

#include <edm4hep/TrackState.h>

namespace opal {

/// Builds an EDM4hep perigee TrackState from the quantities the OPAL ntuple
/// actually stores for a charged track: the Cartesian momentum, the sign of
/// the charge, and the two impact parameters.
///
/// The ntuple keeps no covariance and no curvature, so `omega` is recomputed
/// from pt and the solenoid field, and the covariance matrix is left at zero
/// except for the momentum-error entry that `Dp` provides.
///
/// @param px,py,pz  momentum components [GeV]
/// @param charge    +1 / -1 (see chargeFromIchg)
/// @param d0Cm      transverse impact parameter [cm]
/// @param z0Cm      longitudinal impact parameter [cm]
edm4hep::TrackState makePerigeeState(double px, double py, double pz, float charge,
                                     double d0Cm, double z0Cm);

} // namespace opal

#endif

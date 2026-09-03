/// Checks the perigee conversion against values computed independently from
/// the OPAL field and the ntuple's unit conventions.

#include "opal_edm4hep/Helix.h"
#include "opal_edm4hep/OpalUnits.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

int failures = 0;

void expectNear(const char* what, double got, double want, double tol) {
  if (std::fabs(got - want) > tol) {
    std::printf("FAIL %-28s got %.8g want %.8g\n", what, got, want);
    ++failures;
  }
}

} // namespace

int main() {
  // Ichg is a 0/1 flag, not a signed charge.
  expectNear("charge(Ichg=1)", opal::chargeFromIchg(1), +1.0, 0.0);
  expectNear("charge(Ichg=0)", opal::chargeFromIchg(0), -1.0, 0.0);

  // Fortran arrays are 1-based.
  expectNear("fortranIndex(1)", opal::fortranIndexToOffset(1), 0, 0.0);

  // A 1 GeV positive track along +x, with impact parameters in cm.
  const auto st = opal::makePerigeeState(1.0, 0.0, 0.0, +1.0f, 0.5, -2.0);
  expectNear("phi", st.phi, 0.0, 1e-6);
  expectNear("tanLambda", st.tanLambda, 0.0, 1e-6);
  expectNear("D0 cm->mm", st.D0, 5.0, 1e-5);
  expectNear("Z0 cm->mm", st.Z0, -20.0, 1e-5);
  // R[mm] = 1000*pt/(0.299792458*B) -> omega = 1/R
  const double expectedOmega = 0.299792458 * opal::units::kBFieldTesla / 1000.0;
  expectNear("omega", st.omega, expectedOmega, 1e-9);

  // omega must flip sign with the charge, and scale as 1/pt.
  const auto neg = opal::makePerigeeState(1.0, 0.0, 0.0, -1.0f, 0.0, 0.0);
  expectNear("omega sign", neg.omega, -expectedOmega, 1e-9);
  const auto fast = opal::makePerigeeState(10.0, 0.0, 0.0, +1.0f, 0.0, 0.0);
  expectNear("omega 1/pt", fast.omega, expectedOmega / 10.0, 1e-10);

  // tanLambda for a 45-degree track.
  const auto dip = opal::makePerigeeState(1.0, 0.0, 1.0, +1.0f, 0.0, 0.0);
  expectNear("tanLambda 45deg", dip.tanLambda, 1.0, 1e-6);

  // A zero-momentum entry must not produce inf/nan.
  const auto zero = opal::makePerigeeState(0.0, 0.0, 0.0, +1.0f, 0.0, 0.0);
  if (!std::isfinite(zero.omega)) { std::printf("FAIL omega finite at pt=0\n"); ++failures; }

  std::printf(failures ? "helix_test: %d failure(s)\n" : "helix_test: all checks passed\n",
              failures);
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

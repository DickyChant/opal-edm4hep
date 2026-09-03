#!/bin/bash
# CI build + test recipe. Runs inside a bare AlmaLinux 9 container with
# /cvmfs bind-mounted and the repo at /repo (see .github/workflows/ci.yml).
#
# Reproduce locally on any host with apptainer and /cvmfs:
#   apptainer exec --containall --writable-tmpfs \
#     -B /cvmfs:/cvmfs -B "$PWD":/repo docker://almalinux:9 \
#     bash /repo/.github/scripts/ci-build.sh
set -e

# `source setup.sh` with no arguments passes OUR positional parameters through,
# and key4hep's setup.sh then warns about them. Clear them first.
set --

# OS bits the key4hep (spack) gcc expects from the base system. Everything else
# -- gcc, cmake, ROOT, podio, EDM4hep -- comes from cvmfs.
dnf install -y -q --setopt=install_weak_deps=False glibc-devel zlib-devel

# Stage 2's environment. No -r: take whatever the stable channel resolves to.
# Never `-r latest`; that directory is a stale remnant.
source /cvmfs/sw.hsf.org/key4hep/setup.sh
unset CXXFLAGS CFLAGS LDFLAGS

if [ -z "${KEY4HEP_STACK:-}" ]; then
  echo "ERROR: key4hep setup did not export \$KEY4HEP_STACK" >&2
  exit 1
fi
KEY4HEP_RELEASE=$(echo "${KEY4HEP_STACK}" | sed -n 's|.*/releases/\([^/]*\)/.*|\1|p')
echo "key4hep release ${KEY4HEP_RELEASE}"
# Written before the build so a red run still names the release it tested.
echo "${KEY4HEP_RELEASE}" > /repo/.key4hep-resolved

# Stage 1 lives in a different environment entirely: h2root needs CERNLIB, so
# it exists only in an LCG view, never in the key4hep ROOT. Check it is
# reachable, but do not source it -- the two environments conflict.
LCG_VIEW=${OPAL_LCG_VIEW:-/cvmfs/sft.cern.ch/lcg/views/LCG_107/x86_64-el9-gcc11-opt}
if [ -x "${LCG_VIEW}/bin/h2root" ]; then
  echo "h2root found: ${LCG_VIEW}/bin/h2root"
else
  echo "ERROR: h2root missing at ${LCG_VIEW}/bin/h2root (stage 1 would fail)" >&2
  exit 1
fi

cmake -S /repo/opal_edm4hep -B /repo/build -DCMAKE_BUILD_TYPE=Release
# The converter is built for memory-constrained hosts; -j2 keeps CI honest to
# the same constraint documented in CLAUDE.md.
cmake --build /repo/build -j2
ctest --test-dir /repo/build --output-on-failure

# This script runs as root with the workspace bind-mounted, so build output
# would be root-owned on the host. Hand it back to the checkout's owner.
chown -R "$(stat -c '%u:%g' /repo)" /repo/build /repo/.key4hep-resolved

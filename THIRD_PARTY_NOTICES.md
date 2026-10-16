# Third-Party Notices

This file records third-party and precompiled binary components distributed in
this repository. These notices do not change the license terms of the referenced
components.

## Precompiled Library Families

The repository includes precompiled Linux shared libraries under `lib/aarch64/`,
`lib/aarch64_host/`, `lib/i386/`, and `lib/x86_64/`. Symlinks may point to the versioned library files.

The current four-platform DDS and UniUbi libraries were rebuilt from robotservice
commit `fdda1f339134720bca877ae413c26ba4b05ac1a3` and Build commit
`b21f05bf75c8ffc7e817bfafb72ccdbf7ef923af`, with the public SDK at `f726cfc`
and its atomic Delegate header applied to Build. DDS was built from the updated
source tree before rebuilding the five UniUbi libraries. See
`docs/runtime-manifest.json` for the full input overlays, hashes and validations.

Other third-party binaries retain SDK baseline `dfe4ef3`. The ARM64 external-host
dependencies originally came from internal Build commit
`da59f36bfa834755386d2e405a634c728e6bee48`, using its independent
`3thparty/{cycdds,openssl,zlib}/Lib/aarch64_host/` directories. This provenance
record does not resolve the license/source-package verification items below.

## OpenSSL

- Paths: `lib/*/libssl.so*`, `lib/*/libcrypto.so*`
- Identified version: OpenSSL 1.1.1 family from library names and symbols.
- License: OpenSSL License and SSLeay License for OpenSSL 1.1.1.
- TODO: release blocker. Preserve the complete OpenSSL license notices and
  confirm the exact source package/build provenance for each architecture before
  release.

## Eclipse Cyclone DDS

- Paths: `lib/*/libddsc.so*`, `lib/*/libddscxx.so*`
- Identified version: 0.10.5 from library names.
- License: Eclipse Public License 2.0 and Eclipse Distribution License 1.0, as
  used by Eclipse Cyclone DDS releases.
- The `libddsc` source is `cyclone/dds` in robotservice commit `fdda1f33`.
  This is a locally modified 0.10.5 tree with scoped direct discovery, ingress
  locator handling, SPDP scheduling, discovery pruning and robustness backports;
  it is not an upstream version upgrade. `libddscxx` retains the baseline binary.
- TODO: release blocker. Preserve the complete license notices and confirm the
  upstream source-package provenance of the retained C++ binding before release.

## Eclipse iceoryx

- Paths:
  - `lib/*/libiceoryx_binding_c.so`
  - `lib/*/libiceoryx_hoofs.so`
  - `lib/*/libiceoryx_platform.so`
  - `lib/*/libiceoryx_posh.so`
- License: Apache-2.0, as indicated by iceoryx source headers.
- TODO: release blocker. Confirm the exact source revision, build provenance,
  and whether any local modifications were applied before release.

## zlib

- Paths: `lib/*/libz.so*`
- Identified version: zlib 1.2.11 from library names and embedded strings.
- License: zlib License.
- Copyright notice identified in binaries:
  "Copyright 1995-2017 Jean-loup Gailly and Mark Adler"
- TODO: release blocker. Preserve the complete zlib license notice and confirm
  the exact source package/build provenance for each architecture before release.

## Linux ACL library

- Paths: `lib/*/libacl.so*`
- Identified component: Linux libacl from library names and exported symbols.
- License: TODO: release blocker. Confirm exact source package, version, license,
  and build provenance for each architecture before release.

## Linux attr library

- Paths: `lib/*/libattr.so*`
- Identified component: Linux libattr from library names and exported symbols.
- License: TODO: release blocker. Confirm exact source package, version, license,
  and build provenance for each architecture before release.

## UniUbi Precompiled Libraries

- Paths:
  - `lib/*/librobotMotionSdk.so`
  - `lib/*/libmediaBus.so`
  - `lib/*/libubase.so`
  - `lib/*/libudbus.so`
  - `lib/*/libuutils.so`
- Status: treated as UniUbi-provided precompiled libraries for this release.
- License: TODO: release blocker. Confirm whether these binaries are UniUbi
  proprietary deliverables, Apache-2.0 deliverables, or contain statically linked
  third-party code requiring additional notices before release.

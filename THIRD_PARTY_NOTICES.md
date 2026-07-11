# Third-Party Notices

This file records third-party and precompiled binary components distributed in
this repository. These notices do not change the license terms of the referenced
components.

## Precompiled Library Families

The repository includes precompiled Linux shared libraries under `lib/aarch64/`,
`lib/i386/`, and `lib/x86_64/`. Symlinks may point to the versioned library files.

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
- TODO: release blocker. Confirm the exact source revision, build provenance,
  and whether any local modifications were applied before release.

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

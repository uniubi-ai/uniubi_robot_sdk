# Changelog

## Unreleased

- Rebuild all four runtime bundles from robotservice master `6450481b`, its pinned Build `9b7cfe3`, and public SDK main `25793f1` (tree-equivalent to the English-message rewrite `d7549d7`). Refresh the six rebuilt library families and current validation/provenance records.
- Update version compatibility: firmware `Cyvet-V1.00.003` and newer uses `main`; firmware `Cyvet-V1.00.002` uses SDK tag `Cyvet-V1.00.002`; earlier firmware uses SDK tag `Cyvet-V1.00.001`. Upgrade the runtime libraries and application/Python package together.

- Rebuild all four runtime bundles from robotservice `fdda1f33` and Build `b21f05b`. Update CycloneDDS scoped discovery, scheduling, pruning and robustness fixes; restore default multicast (`AllowMulticast=true`) and support explicit unicast peers. Refresh build provenance and validation records.

- Separately rebuild SDK components with atomic Delegate reference counts from public SDK `f726cfc`; this concurrency fix is not supplied by replacing DDS alone.

- Rebuild all four SDK runtime bundles from robotservice master fb52731b and Build master 05a51e7; refresh runtime provenance and hashes.

- Refresh aarch64_host third-party dependencies from Build da59f36 and rebuild the matched SDK runtime bundle.
- Add a separate aarch64_host runtime bundle for generic ARM64 external hosts, with CMake selection, installed-package support, and remote-audio documentation.
- Enable local and remote MediaBus audio on x86_64, i386, and aarch64; synchronize AudioRawBackStream headers, matched runtimes, and capture/playback examples.
- Expose the paired UWB beacon ID as `sensor.uwb.beaconId` (UWB size 16 bytes; sensor observation size 80 bytes). Requires matching device firmware and rebuilt application/Python bindings.
- Initialize repository structure.
- Add packed `MotionOdometry` protocol data with ABI and type-trait checks.
- Add High Level odometry callback, cached read, and controlled reset APIs.
- Add a read-only C++ odometry example and document epoch, validity, reserved Z fields, and control boundaries.

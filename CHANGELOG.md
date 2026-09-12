# Changelog

## Unreleased

- Expose the paired UWB beacon ID as `sensor.uwb.beaconId` (UWB size 16 bytes; sensor observation size 80 bytes). Requires matching device firmware and rebuilt application/Python bindings.
- Initialize repository structure.
- Add packed `MotionOdometry` protocol data with ABI and type-trait checks.
- Add High Level odometry callback, cached read, and controlled reset APIs.
- Add a read-only C++ odometry example and document epoch, validity, reserved Z fields, and control boundaries.

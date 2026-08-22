# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Versioning note for VCV Rack: plugin versions follow `MAJOR.MINOR.REVISION`, and the `MAJOR` version should match the Rack major version targeted by the plugin. Since this plugin targets Rack 2, versioning starts at `2.x.x`.

## [Unreleased]

## [2.1.0] - 2026-08-22

### Added

- Introduced the 606 Drums module, a seven-voice drum module built on Matthew Fecher's MIT-licensed [606 Inspired Synth Drums](https://github.com/analogcode/606-Inspired-Synth-Drums) DSP, included as a git submodule.
- Added a GitHub Actions workflow that cross-compiles the plugin for Linux, Windows, and both macOS architectures, and publishes a release on version tags.
- Added a License section to the README covering source code, panel graphics, and bundled third-party code.
- Bundled the MIT license of the 606 drum DSP in the distributed package.

### Changed

- Removed the inaccurate `Polyphonic` tag from 606 Drums, which has a single mono mix output.
- Cleaned up plugin metadata: dropped the empty `donateUrl` and the redundant `pluginUrl`, and pointed `manualUrl` at the README.

### Fixed

- Corrected the repository name in the changelog comparison links.

## [2.0.1] - 2026-05-09

### Added

- Added Blinkenlights Plus module documentation to README.

### Changed

- Updated plugin metadata descriptions for modules.
- Improved Blinkenlights Plus UI layout and panel styling.
- Replaced the Blinkenlights Plus color-cycle button with a continuous RGB color knob.

## [2.0.0] - 2026-05-03

### Added

- Initial release.
- Introduced the Blinkenlights module.
- Introduced the Tropical Oscillator module.

[Unreleased]: https://github.com/duddex/duddex-vcv/compare/v2.1.0...HEAD
[2.1.0]: https://github.com/duddex/duddex-vcv/compare/v2.0.1...v2.1.0
[2.0.1]: https://github.com/duddex/duddex-vcv/compare/v2.0.0...v2.0.1
[2.0.0]: https://github.com/duddex/duddex-vcv/releases/tag/v2.0.0

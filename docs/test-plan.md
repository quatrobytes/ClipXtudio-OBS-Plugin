# Public plugin test plan

## Automated gates

1. Configure and compile native core targets.
2. Run native unit and contract tests with CTest.
3. Validate release metadata and installer inputs.
4. Scan tracked files for credentials, private keys, and private-service code.
5. Validate workflow and packaging configuration.

## Manual OBS gates

- Plugin loads in the supported OBS version without missing dependencies.
- Dock opens, resizes, and persists settings.
- Replay Buffer remains user-controlled and reports its real state.
- Manual, voice, and remote capture use only their allowed actions.
- Horizontal and vertical outputs have the expected dimensions and duration.
- Quick editor playback, cuts, captions, and MP4 export complete correctly.
- Network loss, invalid authentication, and inactive entitlements fail safely.
- OBS shutdown leaves no worker, temporary file, or blocked UI thread behind.

## Release gates

- Version metadata agrees across CMake, resources, installer, and changelog.
- Release binaries and installer are signed by the expected publisher.
- Installer is tested on a clean Windows account with a supported OBS build.
- Hashes are published with the release.
- No test token, certificate, private key, or local absolute path is packaged.

An item is PASS only when its evidence was produced for the exact commit and
artifact being released.

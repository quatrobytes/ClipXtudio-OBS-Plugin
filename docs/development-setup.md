# Development setup

## Requirements

- Windows 10/11 x64
- Git
- CMake and Ninja
- Visual Studio 2022 C++ build tools
- OBS Studio development dependencies compatible with this repository
- Qt as required by the selected OBS dependency bundle

Do not place service credentials, signing certificates, private keys, or real
license tokens in the source tree.

## Configure and build

From a Developer PowerShell:

```powershell
git clone https://github.com/quatrobytes/ClipXtudio-OBS-Plugin.git
cd ClipXtudio-OBS-Plugin
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo
```

Preset names can evolve; inspect `CMakePresets.json` when using another host or
generator.

## Core-only tests

The native core can be validated without installing the plugin into OBS:

```powershell
cmake -S . -B build-core -DBUILD_PLUGIN=OFF -DBUILD_TESTS=ON -DBUILD_UI_TESTS=OFF
cmake --build build-core --config Release
ctest --test-dir build-core -C Release --output-on-failure
```

## OBS integration testing

Use a disposable OBS profile and scene collection. Verify plugin loading,
Replay Buffer behavior, horizontal and vertical capture, editor export, local
settings persistence, and clean shutdown. Never test official releases with an
unsigned binary presented as production software.

## Online integrations

Hosted features are exercised through their public HTTPS contracts. A local
plugin build does not require or include the private ClipXtudio service source.
Use test accounts and test tokens only.

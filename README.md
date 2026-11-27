# Gonna Catch You — Skyrim Legendary Edition

A native SKSE plugin for Skyrim Legendary Edition (runtime 1.9.32, SKSE 1.7.3). The goal is to let humanoid NPCs and creatures pursue the player through traversal that ordinary Skyrim pathing does not handle well.

## Project status

This repository is the clean local starting point. The previous prototype is treated as research material; it is not part of this build.

**Current milestone:** read-only LE diagnostics observe humanoid followers and combat-hostile candidates in Skyrim's active high-process actor list. The plugin logs IDs, state, position, distance, height difference, and closing speed once per second. It does not command or alter actors. Follow the two test waves in [docs/FIRST-MILESTONE.md](docs/FIRST-MILESTONE.md) and share `Documents\My Games\Skyrim\SKSE\GonnaCatchYou.log` afterward.

The first movement target is safe humanoid traversal. Creature flight, wall crawling, and burrowing follow as separate movement modules after the humanoid path has been validated. The plugin will coordinate movement ownership so two features cannot control the same actor at once.

## Build and install

Run `tools\Build-LE.ps1` from PowerShell. Provide the extracted SKSE source root with `-SdkRoot`, or provide the official SKSE 1.7.3 source archive with `-SdkArchive`. Visual Studio 2022 Build Tools with the C++ workload is required.

The build writes an installable ZIP to `out\GonnaCatchYou-LE-install.zip`. It contains a `Data` folder at its root, ready for Mod Organizer 2. The packaging script expects the compiled DLL at `build\Release\GonnaCatchYou.dll` and includes the default INI and readme.

Game requirements: Skyrim Legendary Edition 1.9.32 and SKSE 1.7.3.

## References

See [docs/REFERENCES.md](docs/REFERENCES.md) for the source inventory, intended use, and license notes. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the planned module boundaries.

## Logging

The foundation writes `GonnaCatchYou.log` to `Documents\My Games\Skyrim\SKSE` and logs plugin startup plus SKSE game lifecycle messages. If the SKSE directory cannot be opened, the plugin still loads but no log file is written.

## Current behavior

The plugin verifies the Skyrim LE camera-state tables before installing a chained update hook. It refuses the hook if either table is unreadable or contains a non-executable entry. The hook calls the game's original camera update first and performs actor observation at most once per second.

Observation is read-only and enumerates Skyrim LE's active high-process actor handles through `ProcessLists` (`0x012E32E8`; entries at `+0x28`, count at `+0x30`), resolving each handle with SKSE LE's reference lookup (`0x004A9180`). These LE offsets are recorded as confirmed in the supplied CommonLibLE layout data. It records humanoid NPC base forms that are followers marked as player teammates or actors both in combat and hostile to the player. The latter are logged as candidates: this SDK query does not prove that the player's actor is their current combat target. Records include reference/base form IDs, position, 3D and horizontal distance, vertical difference, and distance-closing speed. State transitions and actors leaving the active high-process list are logged too.

The scan is capped at 1,024 active high-process handles and 48 tracked actors. Actors outside the engine's high process are not observed by this milestone. It does not command or modify actors. Creatures are excluded for this humanoid observation milestone and will be handled separately.

## In-game observation waves

1. Load a save with neutral humanoids, provoke combat, then climb/jump to elevated or obstructed places. Look for `HOSTILE_COMBAT_CANDIDATE` entries and continuing samples while they remain active in the high process. Ranged NPCs remain included.
2. Load a save, summon a follower, and traverse obstacles. Look for `HUMANOID_FOLLOWER` and the distance/height samples as the follower moves or falls behind.

Send `Documents\My Games\Skyrim\SKSE\GonnaCatchYou.log` after each wave. No traversal behavior is enabled in this build.

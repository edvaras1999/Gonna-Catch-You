## Logging

The foundation writes `GonnaCatchYou.log` to `Documents\My Games\Skyrim\SKSE` and logs plugin startup plus SKSE game lifecycle messages. If the SKSE directory cannot be opened, the plugin still loads but no log file is written.

## Current behavior

The plugin verifies the Skyrim LE camera-state tables before installing a chained update hook. It refuses the hook if either table is unreadable or contains a non-executable entry. The hook calls the game's original camera update first and performs actor observation at most once per second.

Observation is read-only and limited to humanoid NPC base forms in the player's current loaded cell. It records followers marked as player teammates and actors that are both in combat and hostile to the player. The latter are logged as candidates: this SDK query does not prove that the player's actor is their current combat target. Records include reference/base form IDs, position, 3D and horizontal distance, vertical difference, and distance-closing speed. State transitions and actors leaving the observed cell/scope are logged too.

The scan is capped at 4,096 cell references and 48 tracked actors. It does not command or modify actors. Creatures are excluded for this humanoid observation milestone and will be handled separately. Exterior observation currently enumerates only the player's current cell, so an actor that crosses a cell boundary may leave observation scope.

## In-game observation waves

1. Load a save with neutral humanoids, provoke combat, then climb/jump to elevated or obstructed places. Look for a transition to `HOSTILE_COMBAT_CANDIDATE`, followed by distance and closing-speed samples. Ranged NPCs remain included.
2. Load a save, summon a follower, and traverse obstacles. Look for `HUMANOID_FOLLOWER` and the distance/height samples as the follower moves or falls behind.

Send `Documents\My Games\Skyrim\SKSE\GonnaCatchYou.log` after each wave. No traversal behavior is enabled in this build.


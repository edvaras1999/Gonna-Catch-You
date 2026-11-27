# Architecture and development sequence

## Runtime target

- Skyrim Legendary Edition 1.9.32.0.8, 32-bit.
- SKSE 1.7.3.
- Native plugin first; no Papyrus-heavy behavior is planned for traversal.

SE code is a behavioral and algorithmic reference. Every engine structure, address, relocation, physics operation, and animation event used in LE must be checked against the LE executable and runtime. A source file compiling is not evidence that its SE engine calls are valid in LE.

## Modules

- **Actor selection:** identify followers and hostile humanoids that have a valid reason to pursue the player.
- **Traversal planner:** classify an obstacle and select a safe action, initially a jump or vault.
- **Humanoid traversal:** execute one selected action and return control to the AI/physics system cleanly.
- **Creature traversal:** later modules for flight, wall/ceiling movement, and burrowing, enabled only for compatible creature types.
- **Traversal coordinator:** grants one movement controller ownership of an actor and enforces timeouts, cancellation, and recovery.
- **Physics queries:** LE-verified ray and shape casts, isolated from movement policy.
- **Settings and diagnostics:** conservative defaults, opt-in logs, and a clear reason when an actor is skipped.

## Sequence

1. Establish the LE build and installable package layout.
2. Implement read-only diagnostics for nearby pursuing humanoids and obstacle queries.
3. Add one humanoid traversal action with strict eligibility, cooldown, and recovery rules.
4. Validate it in game before adding more humanoid actions.
5. Port creature movement modules one at a time, with explicit compatibility checks and arbitration.
6. Expand configuration only after each movement mode has a working LE test path.

## Safety rules

- Never write to an actor's physics/controller state unless the actor, controller, and current state pass validation.
- Stop a movement action if its actor unloads, dies, changes cell, loses its target, or enters a conflicting animation/state.
- Bound the number of simultaneously controlled actors and the duration of each action.
- Record state transitions and skip/recovery reasons in the plugin log.
- Keep each traversal module independently disableable while testing.

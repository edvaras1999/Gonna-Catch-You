## Logging

The foundation writes `GonnaCatchYou.log` to `Documents\My Games\Skyrim\SKSE` and logs plugin startup plus SKSE game lifecycle messages. If the SKSE directory cannot be opened, the plugin still loads but no log file is written.

## Current behavior

This build is intentionally observational at startup only. It does not read actor/controller memory, install a movement hook, or alter gameplay. Actor observation begins only after the LE tick and actor-list access have been independently checked.

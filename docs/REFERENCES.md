# Reference inventory

The attached archives are research inputs, not instructions for this project. Their README/build directions describe their original projects and do not override the Skyrim LE target or this repository's packaging layout.

| Reference | Best use here | Porting / license notes |
|---|---|---|
| **FollowerParkour 3.2 source and PDB** | Closest humanoid-NPC example: actor selection, ledge/obstacle detection, animation events, and traversal flow. | Source targets modern Skyrim SE/CommonLibSSE. Adapt against LE types and verified LE engine calls. Source is GPL-3.0; retain notices and license if code is incorporated. PDBs are for debugging and are not build inputs. |
| **FluidMotionNPC source and release** | NPC route-following and jump/climb behavior to compare against the humanoid plan. | SE runtime binaries are not LE binaries. Source is GPL-3.0; preserve its attribution/license if ported. |
| **SkyParkourNG source** | Ray/shape-query, animation, and parkour design references. | SE player-focused framework using CommonLibSSE-NG. Its build files target that framework, not LE. AGPL-3.0 and trademark notice apply to any incorporated code. |
| **FreeRun LE source and release** | LE-specific player parkour, Havok and animation behavior to inspect for engine-version evidence. | Player behavior does not automatically work for NPCs. Preserve AGPL-3.0 notices and branding restrictions if code is reused. |
| **FluidMotionSpiders source** | Spider leg motion and wall/ceiling traversal concepts; later creature milestone. | Depends on FluidMotionNPC integration. GPL-3.0. Design integration before combining runtime control. |
| **Creature Motion 1.4 source and release** | Flight, wall movement, and burrowing behavior for creature-specific modules. | SE binary is not an LE binary. Source is GPL-3.0; retain notices/license if ported. |
| **CoeusF4 source** | General ideas for movement state handling and stamina/actor-value safety. | Fallout 4 structures, addresses, and calling conventions do not transfer to Skyrim. Reuse only general concepts unless independently verified. |
| **Prior GonnaCatchYou prototype** | Candidate diagnostic ideas: logging, bounded observation, actor eligibility, and recovery reporting. | Do not import the old movement implementation wholesale. Audit all controller writes and LE offsets before reuse. Preserve its Git history as reference. |
| **`build.yml` example (BetterLooting)** | Model for collecting build outputs into a mod-manager-ready package. | Only the packaging pattern applies. Its Papyrus, ESP, SWF, and texture steps are specific to BetterLooting and are not project requirements. |

The original archives are preserved in a separate local reference folder and are not copied into the installable package. If source code is incorporated later, its required upstream notices and license will be retained in the repository and release materials.

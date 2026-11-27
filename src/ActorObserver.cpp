#include "ActorObserver.h"

#include <math.h>
#include <map>
#include <set>
#include <stdio.h>
#include <string>

#include "skse/GameAPI.h"
#include "skse/GameFormComponents.h"
#include "skse/GameForms.h"
#include "skse/GameObjects.h"
#include "skse/GameReferences.h"
#include "skse/GameRTTI.h"
#include "skse/GameTypes.h"
#include "skse/NiTypes.h"
#include "skse/Utilities.h"

void LogMessage(const char* message);

namespace
{
    const UInt32 kFollowerFlag = Actor::kFlags_IsPlayerTeammate;
    // Skyrim LE 1.9.32 layout: ProcessLists* at 0x012E32E8, high actor
    // handle array at +0x28 and count at +0x30 (verified in the LE layout data).
    const UInt32 kProcessListsGlobal = 0x012E32E8;
    const UInt32 kHighActorEntriesOffset = 0x28;
    const UInt32 kHighActorCountOffset = 0x30;
    const UInt32 kMaximumHighActorHandles = 1024;
    const UInt32 kMaximumTrackedActors = 48;
    // SKSE LE LookupREFRByHandle from GameReferences.cpp; it returns a retained ref.
    const _LookupREFRByHandle kLookupREFRByHandle = reinterpret_cast<_LookupREFRByHandle>(0x004A9180);

    struct ActorSample
    {
        UInt32 formId;
        UInt32 baseFormId;
        float x;
        float y;
        float z;
        float distance;
        DWORD sampledAt;
        bool follower;
        bool hostileInCombat;
        bool weaponStateKnown;
        bool rangedWeaponEquipped;
        bool fleeing;
        bool dead;
    };

    std::map<UInt32, ActorSample> g_trackedActors;
    std::set<UInt32> g_terminalActors;
    bool g_gameReady = false;
    bool g_limitLogged = false;
    bool g_sourceLogged = false;

    bool IsReadableRange(const void* address, SIZE_T bytes)
    {
        if (!address || bytes == 0) {
            return false;
        }

        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(address, &info, sizeof(info)) || info.State != MEM_COMMIT ||
            (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
            return false;
        }

        const BYTE* start = static_cast<const BYTE*>(address);
        const BYTE* end = start + bytes;
        const BYTE* regionEnd = static_cast<const BYTE*>(info.BaseAddress) + info.RegionSize;
        return end >= start && end <= regionEnd;
    }

    struct HandleReferenceGuard
    {
        explicit HandleReferenceGuard(TESObjectREFR* value) : reference(value) {}
        ~HandleReferenceGuard()
        {
            if (reference) {
                reference->handleRefObject.DecRefHandle();
            }
        }
        TESObjectREFR* reference;
    };

    float Distance3D(float ax, float ay, float az, float bx, float by, float bz)
    {
        const float dx = ax - bx;
        const float dy = ay - by;
        const float dz = az - bz;
        return sqrtf(dx * dx + dy * dy + dz * dz);
    }

    bool HasRangedWeaponEquipped(Actor* actor, bool& stateKnown)
    {
        stateKnown = false;
        if (!actor || !IsReadableRange(actor, sizeof(Actor)) || !actor->processManager ||
            !IsReadableRange(actor->processManager, sizeof(ActorProcessManager))) {
            return false;
        }

        stateKnown = true;
        for (UInt32 hand = 0; hand < 2; ++hand) {
            TESForm* equipped = actor->processManager->equippedObject[hand];
            if (!equipped || !IsReadableRange(equipped, sizeof(TESForm)) ||
                equipped->formType != TESObjectWEAP::kTypeID) {
                continue;
            }

            TESObjectWEAP* weapon = DYNAMIC_CAST(equipped, TESForm, TESObjectWEAP);
            if (!weapon || !IsReadableRange(weapon, sizeof(TESObjectWEAP))) {
                continue;
            }

            const UInt8 type = weapon->gameData.type;
            if (type == TESObjectWEAP::GameData::kType_Bow ||
                type == TESObjectWEAP::GameData::kType_CrossBow ||
                type == TESObjectWEAP::GameData::kType_Bow2 ||
                type == TESObjectWEAP::GameData::kType_CBow) {
                return true;
            }
        }
        return false;
    }

    bool IsFleeing(Actor* actor)
    {
        if (!actor || !IsReadableRange(actor, sizeof(Actor)) || !actor->processManager ||
            !IsReadableRange(actor->processManager, sizeof(ActorProcessManager))) {
            return false;
        }

        TESPackage* package = actor->processManager->package;
        if (!package || !IsReadableRange(package, sizeof(TESPackage)) ||
            package->formType != TESForm::kFormType_Package) {
            return false;
        }
        return package->type == TESPackage::kPackageType_Flee ||
            package->type == TESPackage::kPackageType_FleeNotCombat;
    }

    const char* StateName(const ActorSample& sample)
    {
        if (sample.dead) return "DEAD";
        if (sample.fleeing) return "FLEEING";
        if (sample.follower && sample.hostileInCombat) return "FOLLOWER+HOSTILE_COMBAT";
        if (sample.follower) return "FOLLOWER";
        if (sample.hostileInCombat && sample.rangedWeaponEquipped) return "HOSTILE_RANGED_COMBAT";
        if (sample.hostileInCombat && !sample.weaponStateKnown) return "HOSTILE_WEAPON_STATE_UNKNOWN";
        if (sample.hostileInCombat) return "HOSTILE_MELEE_COMBAT_CANDIDATE";
        return "OUT_OF_SCOPE";
    }

    void LogActor(const char* eventName, const ActorSample& sample,
        float horizontalDistance, float verticalDifference, float closingSpeed)
    {
        char line[512]{};
        sprintf_s(line,
            "OBS %s state=%s rangedWeaponEquipped=%u ref=%08X base=%08X pos=(%.0f,%.0f,%.0f) distance3d=%.0f horizontal=%.0f dz=%.0f closingUnitsPerSec=%.1f",
            eventName, StateName(sample), sample.rangedWeaponEquipped ? 1 : 0, sample.formId,
            sample.baseFormId, sample.x, sample.y, sample.z, sample.distance,
            horizontalDistance, verticalDifference, closingSpeed);
        LogMessage(line);
    }
}

namespace ActorObserver
{
    void SetGameReady(bool ready)
    {
        g_gameReady = ready;
    }

    void Reset(const char* reason)
    {
        if (!g_trackedActors.empty()) {
            char line[192]{};
            sprintf_s(line, "OBS reset reason=%s previousTracked=%u", reason ? reason : "unspecified",
                static_cast<UInt32>(g_trackedActors.size()));
            LogMessage(line);
        }
        g_trackedActors.clear();
        g_terminalActors.clear();
        g_limitLogged = false;
        g_sourceLogged = false;
    }

    void Scan()
    {
        if (!g_gameReady || !g_thePlayer || !*g_thePlayer) {
            return;
        }

        PlayerCharacter* player = *g_thePlayer;
        if (!IsReadableRange(reinterpret_cast<const void*>(kProcessListsGlobal), sizeof(UInt32))) {
            return;
        }

        void* processLists = *reinterpret_cast<void**>(kProcessListsGlobal);
        if (!IsReadableRange(processLists, kHighActorCountOffset + sizeof(UInt32))) {
            return;
        }

        UInt32* actorHandles = *reinterpret_cast<UInt32**>(
            static_cast<UInt8*>(processLists) + kHighActorEntriesOffset);
        const UInt32 actorHandleCount = *reinterpret_cast<UInt32*>(
            static_cast<UInt8*>(processLists) + kHighActorCountOffset);
        if (actorHandleCount > 0 && !actorHandles) {
            return;
        }

        const UInt32 handleCount = actorHandleCount < kMaximumHighActorHandles
            ? actorHandleCount : kMaximumHighActorHandles;
        if (handleCount > 0 && !IsReadableRange(actorHandles, sizeof(UInt32) * handleCount)) {
            return;
        }
        if (!g_sourceLogged) {
            LogMessage("OBS source=LE ProcessLists high-process actor handles; scanning active actors outside the player's cell.");
            g_sourceLogged = true;
        }
        if (actorHandleCount > kMaximumHighActorHandles && !g_limitLogged) {
            LogMessage("OBS high-process actor handle limit reached; additional active handles are skipped.");
            g_limitLogged = true;
        }

        std::set<UInt32> seenThisScan;
        std::set<UInt32> seenInProcessList;
        for (UInt32 index = 0; index < handleCount; ++index) {
            UInt32 handle = actorHandles[index];
            TESObjectREFR* reference = nullptr;
            if (!handle || !kLookupREFRByHandle(&handle, &reference) || !reference) {
                continue;
            }
            HandleReferenceGuard referenceGuard(reference);
            if (reference == player) {
                continue;
            }

            Actor* actor = DYNAMIC_CAST(reference, TESObjectREFR, Actor);
            if (!actor || !actor->formID) {
                continue;
            }

            // Humanoid actors use TESNPC as their base form; creature base
            // forms are deliberately left for the later creature milestone.
            if (!actor->baseForm || !DYNAMIC_CAST(actor->baseForm, TESForm, TESNPC)) {
                continue;
            }
            seenInProcessList.insert(actor->formID);

            const bool follower = (actor->flags1 & kFollowerFlag) != 0;
            const bool hostileInCombat = actor->IsInCombat() && CALL_MEMBER_FN(actor, IsHostileToActor)(player);
            if (!follower && !hostileInCombat) {
                continue;
            }

            if (g_trackedActors.find(actor->formID) == g_trackedActors.end() &&
                g_trackedActors.size() >= kMaximumTrackedActors) {
                if (!g_limitLogged) {
                    LogMessage("OBS tracked actor limit reached; additional eligible active actors are being skipped.");
                    g_limitLogged = true;
                }
                continue;
            }

            const float dx = actor->pos.x - player->pos.x;
            const float dy = actor->pos.y - player->pos.y;
            const float dz = actor->pos.z - player->pos.z;
            const float horizontalDistance = sqrtf(dx * dx + dy * dy);
            const float distance = sqrtf(dx * dx + dy * dy + dz * dz);
            const DWORD now = GetTickCount();
            ActorSample sample{};
            sample.formId = actor->formID;
            sample.baseFormId = actor->baseForm ? actor->baseForm->formID : 0;
            sample.x = actor->pos.x;
            sample.y = actor->pos.y;
            sample.z = actor->pos.z;
            sample.distance = distance;
            sample.sampledAt = now;
            sample.follower = follower;
            sample.hostileInCombat = hostileInCombat;
            sample.rangedWeaponEquipped = HasRangedWeaponEquipped(actor, sample.weaponStateKnown);
            sample.fleeing = IsFleeing(actor);
            sample.dead = actor->IsDead(1);

            if (sample.dead || sample.fleeing) {
                const bool firstTerminalObservation = g_terminalActors.insert(sample.formId).second;
                auto terminal = g_trackedActors.find(sample.formId);
                if (terminal != g_trackedActors.end()) {
                    LogActor(sample.dead ? "EXIT_DEAD" : "EXIT_FLEEING", sample,
                        horizontalDistance, dz, 0.0f);
                    g_trackedActors.erase(terminal);
                } else if (firstTerminalObservation) {
                    LogActor(sample.dead ? "SKIP_DEAD" : "SKIP_FLEEING", sample,
                        horizontalDistance, dz, 0.0f);
                }
                continue;
            }
            g_terminalActors.erase(sample.formId);

            seenThisScan.insert(sample.formId);
            auto previous = g_trackedActors.find(sample.formId);
            if (previous == g_trackedActors.end()) {
                LogActor("ENTER", sample, horizontalDistance, dz, 0.0f);
                g_trackedActors[sample.formId] = sample;
                continue;
            }

            const ActorSample old = previous->second;
            const DWORD elapsed = now - old.sampledAt;
            const float closingSpeed = elapsed > 0 ? (old.distance - distance) * 1000.0f / elapsed : 0.0f;
            if (old.follower != follower || old.hostileInCombat != hostileInCombat ||
                old.weaponStateKnown != sample.weaponStateKnown ||
                old.rangedWeaponEquipped != sample.rangedWeaponEquipped) {
                LogActor("STATE_CHANGE", sample, horizontalDistance, dz, closingSpeed);
            }
            LogActor("SAMPLE", sample, horizontalDistance, dz, closingSpeed);
            previous->second = sample;
        }

        for (auto it = g_trackedActors.begin(); it != g_trackedActors.end();) {
            if (seenThisScan.find(it->first) == seenThisScan.end()) {
                LogActor("EXIT_OBSERVATION_SCOPE", it->second, 0.0f, 0.0f, 0.0f);
                it = g_trackedActors.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = g_terminalActors.begin(); it != g_terminalActors.end();) {
            if (seenInProcessList.find(*it) == seenInProcessList.end()) {
                it = g_terminalActors.erase(it);
            } else {
                ++it;
            }
        }
    }
}

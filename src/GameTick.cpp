#include "GameTick.h"

#include <string>
#include <stdio.h>

#include "ActorObserver.h"
#include "common/IPrefix.h"
#include "skse/GameTypes.h"

void LogMessage(const char* message);

namespace
{
    // Skyrim LE 1.9.32.0.8 camera-state vtables; also used by FreeRun LE.
    UInt32* const kThirdPersonStateVTable = reinterpret_cast<UInt32*>(0x010E29EC);
    UInt32* const kFirstPersonStateVTable = reinterpret_cast<UInt32*>(0x010E30C8);
    const UInt32 kUpdateSlot = 3;
    const DWORD kObservationIntervalMs = 1000;

    typedef void (__thiscall* CameraUpdate)(void* state, void* nextState);
    CameraUpdate g_originalThirdPersonUpdate = nullptr;
    CameraUpdate g_originalFirstPersonUpdate = nullptr;
    DWORD g_lastObservationTick = 0;
    bool g_hooksInstalled = false;

    bool IsReadable(const void* address, SIZE_T bytes)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(address, &info, sizeof(info))) {
            return false;
        }
        if (info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
            return false;
        }
        const BYTE* end = static_cast<const BYTE*>(address) + bytes;
        const BYTE* regionEnd = static_cast<const BYTE*>(info.BaseAddress) + info.RegionSize;
        return end >= static_cast<const BYTE*>(address) && end <= regionEnd;
    }

    bool IsExecutableAddress(UInt32 address)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(reinterpret_cast<const void*>(address), &info, sizeof(info))) {
            return false;
        }
        if (info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
            return false;
        }
        const DWORD protection = info.Protect & 0xFF;
        return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
            protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
    }

    bool IsVerifiedCameraTable(UInt32* table, const char* label)
    {
        // Check the complete six-entry table used by the LE signature audit.
        if (!IsReadable(table, sizeof(UInt32) * 6)) {
            char message[192]{};
            sprintf_s(message, "%s camera vtable is not readable; observation hook was refused.", label);
            LogMessage(message);
            return false;
        }

        for (UInt32 i = 0; i < 6; ++i) {
            if (!IsExecutableAddress(table[i])) {
                char message[224]{};
                sprintf_s(message, "%s camera vtable slot %u is not executable; observation hook was refused.", label, i);
                LogMessage(message);
                return false;
            }
        }
        return true;
    }

    bool ReplaceUpdate(UInt32* table, void* replacement, CameraUpdate* original)
    {
        if (!table || !replacement || !original || !IsReadable(&table[kUpdateSlot], sizeof(UInt32))) {
            return false;
        }

        *original = reinterpret_cast<CameraUpdate>(table[kUpdateSlot]);
        DWORD oldProtection = 0;
        if (!VirtualProtect(&table[kUpdateSlot], sizeof(UInt32), PAGE_EXECUTE_READWRITE, &oldProtection)) {
            *original = nullptr;
            return false;
        }

        table[kUpdateSlot] = reinterpret_cast<UInt32>(replacement);
        DWORD ignoredProtection = 0;
        const BOOL restored = VirtualProtect(&table[kUpdateSlot], sizeof(UInt32), oldProtection, &ignoredProtection);
        if (restored != FALSE) {
            return true;
        }

        DWORD retryProtection = 0;
        if (VirtualProtect(&table[kUpdateSlot], sizeof(UInt32), PAGE_EXECUTE_READWRITE, &retryProtection)) {
            table[kUpdateSlot] = reinterpret_cast<UInt32>(*original);
            VirtualProtect(&table[kUpdateSlot], sizeof(UInt32), retryProtection, &ignoredProtection);
        }
        *original = nullptr;
        return false;
    }

    bool RestoreUpdate(UInt32* table, CameraUpdate original)
    {
        if (!table || !original) {
            return false;
        }

        DWORD oldProtection = 0;
        if (!VirtualProtect(&table[kUpdateSlot], sizeof(UInt32), PAGE_EXECUTE_READWRITE, &oldProtection)) {
            return false;
        }
        table[kUpdateSlot] = reinterpret_cast<UInt32>(original);
        DWORD ignoredProtection = 0;
        return VirtualProtect(&table[kUpdateSlot], sizeof(UInt32), oldProtection, &ignoredProtection) != FALSE;
    }

    void __fastcall HookThirdPersonUpdate(void* state, void*, void* nextState)
    {
        if (g_originalThirdPersonUpdate) {
            g_originalThirdPersonUpdate(state, nextState);
        }
        GameTick::OnCameraUpdate();
    }

    void __fastcall HookFirstPersonUpdate(void* state, void*, void* nextState)
    {
        if (g_originalFirstPersonUpdate) {
            g_originalFirstPersonUpdate(state, nextState);
        }
        GameTick::OnCameraUpdate();
    }
}

namespace GameTick
{
    bool InstallHooks()
    {
        if (g_hooksInstalled) {
            return true;
        }
        if (sizeof(void*) != 4 ||
            !IsVerifiedCameraTable(kThirdPersonStateVTable, "Third-person") ||
            !IsVerifiedCameraTable(kFirstPersonStateVTable, "First-person")) {
            LogMessage("LE camera update verification failed; no actor scan will run.");
            return false;
        }

        const bool thirdInstalled = ReplaceUpdate(kThirdPersonStateVTable,
            reinterpret_cast<void*>(&HookThirdPersonUpdate), &g_originalThirdPersonUpdate);
        if (!thirdInstalled) {
            LogMessage("Could not install third-person camera observer hook; actor scan is disabled.");
            return false;
        }

        const bool firstInstalled = ReplaceUpdate(kFirstPersonStateVTable,
            reinterpret_cast<void*>(&HookFirstPersonUpdate), &g_originalFirstPersonUpdate);
        if (!firstInstalled) {
            RestoreUpdate(kThirdPersonStateVTable, g_originalThirdPersonUpdate);
            g_originalThirdPersonUpdate = nullptr;
            LogMessage("Could not install first-person camera observer hook; third-person hook was rolled back.");
            return false;
        }

        g_hooksInstalled = true;
        LogMessage("Verified Skyrim LE camera update hooks installed; actor observation runs at most once per second.");
        return true;
    }

    void OnCameraUpdate()
    {
        const DWORD now = GetTickCount();
        if (static_cast<DWORD>(now - g_lastObservationTick) < kObservationIntervalMs) {
            return;
        }
        g_lastObservationTick = now;
        ActorObserver::Scan();
    }
}


#include <Windows.h>
#include <ShlObj.h>
#include <stdio.h>

#include "common/ITypes.h"
#include "skse/PluginAPI.h"
#include "skse/skse_version.h"

namespace
{
    constexpr UInt32 kPluginVersion = 1;
    constexpr UInt32 kMinimumMessagingVersion = SKSEMessagingInterface::kInterfaceVersion;

    PluginHandle g_pluginHandle = kPluginHandle_Invalid;
    HANDLE g_logFile = INVALID_HANDLE_VALUE;

    bool OpenLog()
    {
        if (g_logFile != INVALID_HANDLE_VALUE) {
            return true;
        }

        PWSTR documents = nullptr;
        const HRESULT result = SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_CREATE, nullptr, &documents);
        if (FAILED(result) || !documents) {
            return false;
        }

        wchar_t logDirectory[MAX_PATH * 4] = {};
        wchar_t logPath[MAX_PATH * 4] = {};
        const int dirLength = _snwprintf_s(logDirectory, _countof(logDirectory), _TRUNCATE,
            L"%s\\My Games\\Skyrim\\SKSE", documents);
        CoTaskMemFree(documents);
        if (dirLength < 0) {
            return false;
        }

        const int createResult = SHCreateDirectoryExW(nullptr, logDirectory, nullptr);
        if (createResult != ERROR_SUCCESS && createResult != ERROR_ALREADY_EXISTS && createResult != ERROR_FILE_EXISTS) {
            return false;
        }

        const int pathLength = _snwprintf_s(logPath, _countof(logPath), _TRUNCATE,
            L"%s\\GonnaCatchYou.log", logDirectory);
        if (pathLength < 0) {
            return false;
        }

        g_logFile = CreateFileW(logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        return g_logFile != INVALID_HANDLE_VALUE;
    }

    void Log(const char* message)
    {
        if (!message || !OpenLog()) {
            return;
        }

        SYSTEMTIME now{};
        GetLocalTime(&now);
        char line[768] = {};
        const int length = sprintf_s(line, sizeof(line),
            "[%04u-%02u-%02u %02u:%02u:%02u.%03u] %s\r\n",
            now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
            now.wMilliseconds, message);
        if (length > 0) {
            DWORD written = 0;
            WriteFile(g_logFile, line, static_cast<DWORD>(length), &written, nullptr);
        }
    }

    void OnSKSEMessage(SKSEMessagingInterface::Message* message)
    {
        if (!message) {
            return;
        }

        switch (message->type) {
        case SKSEMessagingInterface::kMessage_DataLoaded:
            Log("SKSE DataLoaded; startup-only foundation active. No actor state is read or changed.");
            break;
        case SKSEMessagingInterface::kMessage_NewGame:
            Log("SKSE NewGame received.");
            break;
        case SKSEMessagingInterface::kMessage_PreLoadGame:
            Log("SKSE PreLoadGame received.");
            break;
        case SKSEMessagingInterface::kMessage_PostLoadGame:
            Log("SKSE PostLoadGame received.");
            break;
        default:
            break;
        }
    }
}

extern "C"
{
    bool SKSEPlugin_Query(const SKSEInterface* skse, PluginInfo* info)
    {
        if (!info) {
            return false;
        }

        info->infoVersion = PluginInfo::kInfoVersion;
        info->name = "GonnaCatchYou";
        info->version = kPluginVersion;

        if (!skse || skse->isEditor) {
            return false;
        }
        if (skse->runtimeVersion != RUNTIME_VERSION_1_9_32_0) {
            return false;
        }
        return skse->skseVersion == PACKED_SKSE_VERSION;
    }

    bool SKSEPlugin_Load(const SKSEInterface* skse)
    {
        if (!skse || !skse->QueryInterface || !skse->GetPluginHandle) {
            return false;
        }

        g_pluginHandle = skse->GetPluginHandle();
        if (g_pluginHandle == kPluginHandle_Invalid) {
            return false;
        }

        Log("Plugin load started; Skyrim LE 1.9.32 / SKSE 1.7.3 accepted.");

        auto* messaging = static_cast<SKSEMessagingInterface*>(
            skse->QueryInterface(kInterface_Messaging));
        if (!messaging || messaging->interfaceVersion < kMinimumMessagingVersion || !messaging->RegisterListener) {
            Log("Plugin load aborted: SKSE messaging interface is unavailable or too old.");
            return false;
        }

        if (!messaging->RegisterListener(g_pluginHandle, "SKSE", OnSKSEMessage)) {
            Log("Plugin load aborted: could not register for SKSE lifecycle messages.");
            return false;
        }

        Log("Plugin loaded. This foundation does not yet inspect or control actors.");
        return true;
    }
}

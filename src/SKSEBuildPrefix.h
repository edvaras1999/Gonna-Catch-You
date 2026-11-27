#pragma once

// Keep the useful legacy SKSE precompiled-header setup without forcing
// _WIN32_WINNT=0x0501, which hides Vista+ Windows APIs from the plugin.
#pragma warning(disable: 4018 4244 4267 4305 4288 4312 4311)

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>

#include "common/ITypes.h"
#include "common/IErrors.h"
#include "common/IDynamicCreate.h"
#include "common/IDebugLog.h"
#include "common/ISingleton.h"

#include <winsock2.h>
#include <Windows.h>

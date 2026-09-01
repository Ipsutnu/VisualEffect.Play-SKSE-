/*
 * Copyright (c) 2026 Preguissoso
 *
 * This file is part of VisualEffect.Play(SKSE).
 *
 * The source code is available for viewing and reference purposes only.
 * Modification, redistribution, forking, and creation of derivative
 * works are not permitted without prior written permission.
 *
 * See LICENSE for the full license terms.
 */

#pragma once

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <atomic>
#include <thread>
#include <algorithm>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <queue>

#include <ShlObj.h>
#include <format>
#include <filesystem>

#include <cstdlib>
#include <cstring>
#include <cctype>
#include <iomanip>

#include <fstream>
#include <mutex>

#include <imgui.h>
#include "imgui_internal.h"
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>

#define IMGUI_DEFINE_MATH_OPERATORS

#include <MinHook.h>

#include <windows.h>

#include <wincodec.h>

#include <chrono>
#include <sstream>

#include <d3d11.h>
#include <dxgi.h>

#include "EffectDatabase.h"
#include "EffectControl.h"

#pragma comment(lib, "windowscodecs.lib")

extern char g_openMenuKeyBuffer[32];
extern char g_movementKeyBuffer[32];

extern bool g_showKeyConfig;

extern int g_toggleKey;
extern int g_movementToggleKey;

extern bool g_takeScreenshot;
extern std::string g_screenshotNotification;
extern float g_screenshotNotificationTime;

namespace {
    void SetImGuiInputContext(bool enabled) {
        // lógica interna
    }
}

inline bool ContainsCaseInsensitive(std::string_view haystack, std::string_view needle);

extern bool g_showTestWindow;

void IncreaseEffectDuration();
void DecreaseEffectDuration();

extern std::vector<EffectDatabase::EffectEntry>& GetEffectsForType(EffectType type);


using namespace std::literals;

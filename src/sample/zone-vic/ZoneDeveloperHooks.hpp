#pragma once

#include "cISC4ZoneDeveloper.h"

#include <cstdint>

class cISC4NetworkTool;
class cISC4NetworkManager;
class cISC4City;

struct ZoneDeveloperDebugState
{
    bool available = false;
    uintptr_t self = 0;
    uintptr_t backend44 = 0;
    uintptr_t backend48 = 0;
    uintptr_t backend4C = 0;
    uintptr_t backend50 = 0;
    uintptr_t backend44VTable = 0;
    uintptr_t backend48VTable = 0;
    uintptr_t backend4CVTable = 0;
    uintptr_t backend50VTable = 0;
    int32_t lotWidth = 0;
    int32_t lotHeight = 0;
    int32_t minWidth = 0;
    int32_t minHeight = 0;
    int32_t streetInterval = 0;
    int32_t zoneTypeRaw = 0;
    int32_t allowedRegion = 0;
    int32_t focusPoint = 0;
    int32_t orientationPrimary = 0;
    int32_t orientationSecondary = 0;
    uint8_t flag98 = 0;
    uint8_t flag99 = 0;
    uint8_t flag9A = 0;
    uint8_t flag9B = 0;
    uint8_t flag9C = 0;
};

struct NetworkToolDebugState
{
    bool available = false;
    uintptr_t self = 0;
    uintptr_t vtable = 0;
    int32_t toolType = 0;
    uint32_t blockedMask = 0;
    uint8_t mode50 = 0;
    uint8_t flag52 = 0;
    uint8_t flag53 = 0;
};

namespace ZoneDeveloperHooks
{
bool SupportsCurrentVersion();
bool Install();
void Uninstall();
bool IsInstalled();
bool ClearLiveHighlight(cISC4ZoneDeveloper* zoneDeveloper);
bool ReadDebugState(cISC4ZoneDeveloper* zoneDeveloper, ZoneDeveloperDebugState& state);
bool ApplyDebugPokes(cISC4ZoneDeveloper* zoneDeveloper);
bool ReadNetworkToolDebugState(cISC4NetworkTool* networkTool, NetworkToolDebugState& state);
bool ApplyNetworkManagerToolOverrides(cISC4ZoneDeveloper* zoneDeveloper, cISC4NetworkManager* networkManager);
bool CreateFreshNetworkTool(int32_t networkType, cISC4NetworkTool*& outTool);
bool InitFreshNetworkTool(cISC4NetworkTool* tool);
void ResetFreshNetworkTool(cISC4NetworkTool* tool);
void DestroyFreshNetworkTool(cISC4NetworkTool*& tool);
bool PlaceIntersectionByRuleId(cISC4City* city, int32_t cellX, int32_t cellZ, uint32_t ruleId, bool commit, int32_t* outCost = nullptr);
}

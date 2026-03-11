// ReSharper disable CppDFAConstantConditions
// ReSharper disable CppDFAUnreachableCode
#include "sample/zone-vic/ZoneViewInputControl.hpp"

#include "sample/zone-vic/ZoneDeveloperHooks.hpp"
#include "sample/zone-vic/ZoneTuningSettings.hpp"

#include "GZServPtrs.h"
#include "SC4CellRegion.h"
#include "SC4Point.h"
#include "cISC4App.h"
#include "cISC4City.h"
#include "cISC4NetworkLot.h"
#include "cISC4NetworkLotManager.h"
#include "cISC4NetworkManager.h"
#include "cISC4NetworkOccupant.h"
#include "cISC4NetworkTool.h"
#include "cISC4ZoneDeveloper.h"
#include "cISC4ZoneManager.h"
#include "cRZBaseString.h"
#include "utils/Logger.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
    constexpr uint32_t kCursorTextId = 0x57C44A11;
    constexpr uint32_t kCursorTextPriority = 160;

    bool HasZoningNetworkAtCell(cISC4ZoneDeveloper* zoneDeveloper, const int32_t x, const int32_t z)
    {
        return zoneDeveloper &&
               (zoneDeveloper->ExistsNetworkOfType(x, z, 0x8) ||
                zoneDeveloper->ExistsNetworkOfType(x, z, 0x800) ||
                zoneDeveloper->ExistsNetworkOfType(x, z, 0x808));
    }

    void QueueRetrofitForNewNetworkTiles(cISC4City* city,
                                         cISC4ZoneDeveloper* zoneDeveloper,
                                         const int32_t minX,
                                         const int32_t minZ,
                                         const int32_t maxX,
                                         const int32_t maxZ,
                                         const std::vector<uint8_t>& preNetworkMap)
    {
        if (!city || !zoneDeveloper || preNetworkMap.empty()) {
            return;
        }

        cISC4NetworkLotManager* networkLotManager = city->GetNetworkLotManager();
        cISC4NetworkManager* networkManager = city->GetNetworkManager();
        LOG_INFO("ZoneViewInputControl: retrofit lookup lotManager=0x{:08X} networkManager=0x{:08X}",
                 reinterpret_cast<uintptr_t>(networkLotManager),
                 reinterpret_cast<uintptr_t>(networkManager));
        if (!networkLotManager || !networkManager) {
            return;
        }

        const int32_t width = maxX - minX + 1;
        std::unordered_set<uintptr_t> seenOccupants;
        int32_t newNetworkCells = 0;
        int32_t queuedOccupants = 0;

        for (int32_t z = minZ; z <= maxZ; ++z) {
            for (int32_t x = minX; x <= maxX; ++x) {
                const size_t index = static_cast<size_t>((z - minZ) * width + (x - minX));
                const bool hadNetworkBefore = preNetworkMap[index] != 0;
                if (hadNetworkBefore || !HasZoningNetworkAtCell(zoneDeveloper, x, z)) {
                    continue;
                }

                ++newNetworkCells;
                cISC4NetworkLot* networkLot = networkLotManager->GetNetworkLot(x, z);
                LOG_INFO("ZoneViewInputControl: new network tile {},{} lot=0x{:08X}",
                         x,
                         z,
                         reinterpret_cast<uintptr_t>(networkLot));
                if (!networkLot) {
                    continue;
                }

                cISC4NetworkOccupant* networkOccupant = networkLot->GetNetworkOccupant();
                LOG_INFO("ZoneViewInputControl: tile {},{} occupant=0x{:08X}",
                         x,
                         z,
                         reinterpret_cast<uintptr_t>(networkOccupant));
                if (!networkOccupant) {
                    continue;
                }

                const uintptr_t occupantKey = reinterpret_cast<uintptr_t>(networkOccupant);
                if (!seenOccupants.insert(occupantKey).second) {
                    continue;
                }

                networkOccupant->SetRetrofitFlag(true);
                const bool retrofitQueued = networkManager->AddToRetrofitList(networkOccupant);
                LOG_INFO("ZoneViewInputControl: AddToRetrofitList cell={},{} piece=0x{:08X} queued={}",
                         x,
                         z,
                         networkOccupant->PieceId(),
                         retrofitQueued ? 1 : 0);
                if (retrofitQueued) {
                    ++queuedOccupants;
                }
            }
        }

        LOG_INFO("ZoneViewInputControl: retrofit summary new_cells={} unique_occupants={} queued={}",
                 newNetworkCells,
                 static_cast<int32_t>(seenOccupants.size()),
                 queuedOccupants);
    }

    bool OverrideNewNetworkComponents(cISC4City* city,
                                      cISC4ZoneDeveloper* zoneDeveloper,
                                      const int32_t minX,
                                      const int32_t minZ,
                                      const int32_t maxX,
                                      const int32_t maxZ,
                                      const int32_t anchorX,
                                      const int32_t anchorZ,
                                      const std::vector<uint8_t>& preNetworkMap,
                                      const int32_t networkToolType,
                                      const uint32_t intersectionRuleId)
    {
        if (!city || !zoneDeveloper || preNetworkMap.empty()) {
            return false;
        }

        const int32_t width = maxX - minX + 1;
        const int32_t height = maxZ - minZ + 1;
        auto isNewNetworkCell = [&](const int32_t x, const int32_t z) -> bool {
            if (x < minX || x > maxX || z < minZ || z > maxZ) {
                return false;
            }
            const size_t index = static_cast<size_t>((z - minZ) * width + (x - minX));
            return preNetworkMap[index] == 0 && HasZoningNetworkAtCell(zoneDeveloper, x, z);
        };

        const auto networkType = static_cast<cISC4NetworkOccupant::eNetworkType>(networkToolType);
        auto drawRun = [&](const SC4Point<uint32_t>& start, const SC4Point<uint32_t>& end) -> bool {
            cISC4NetworkTool* tool = nullptr;
            if (!ZoneDeveloperHooks::CreateFreshNetworkTool(networkToolType, tool) || !tool) {
                LOG_WARN("ZoneViewInputControl: failed to create tool {}", networkToolType);
                return false;
            }

            ZoneDeveloperHooks::InitFreshNetworkTool(tool);
            ZoneDeveloperHooks::ResetFreshNetworkTool(tool);
            const bool drew = tool->DrawNetworkLine(start, end, false, networkType) != 0;
            LOG_INFO("ZoneViewInputControl: DrawNetworkLine tool={} start={},{} end={},{} drew={}",
                     networkToolType,
                     start.x,
                     start.y,
                     end.x,
                     end.y,
                     drew ? 1 : 0);
            if (drew) {
                tool->PlaceAllSegments(true, false, true, networkType);
                LOG_INFO("ZoneViewInputControl: PlaceAllSegments committed for tool={}", networkToolType);
            }
            ZoneDeveloperHooks::DestroyFreshNetworkTool(tool);
            return drew;
        };

        std::vector<uint8_t> visited(static_cast<size_t>(width * height), 0);
        bool anyDrew = false;
        int32_t componentCount = 0;

        for (int32_t seedZ = minZ; seedZ <= maxZ; ++seedZ) {
            for (int32_t seedX = minX; seedX <= maxX; ++seedX) {
                const size_t seedIndex = static_cast<size_t>((seedZ - minZ) * width + (seedX - minX));
                if (visited[seedIndex] || !isNewNetworkCell(seedX, seedZ)) {
                    continue;
                }

                ++componentCount;
                std::vector<SC4Point<uint32_t>> stack;
                std::vector<SC4Point<uint32_t>> cells;
                std::vector<uint8_t> componentMask(static_cast<size_t>(width * height), 0);
                stack.push_back({static_cast<uint32_t>(seedX), static_cast<uint32_t>(seedZ)});
                visited[seedIndex] = 1;

                while (!stack.empty()) {
                    const SC4Point<uint32_t> point = stack.back();
                    stack.pop_back();
                    cells.push_back(point);
                    const int32_t x = static_cast<int32_t>(point.x);
                    const int32_t z = static_cast<int32_t>(point.y);
                    componentMask[static_cast<size_t>((z - minZ) * width + (x - minX))] = 1;

                    constexpr int32_t kDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                    for (const auto& dir : kDirs) {
                        const int32_t nx = x + dir[0];
                        const int32_t nz = z + dir[1];
                        if (nx < minX || nx > maxX || nz < minZ || nz > maxZ) {
                            continue;
                        }
                        const size_t nIndex = static_cast<size_t>((nz - minZ) * width + (nx - minX));
                        if (visited[nIndex] || !isNewNetworkCell(nx, nz)) {
                            continue;
                        }
                        visited[nIndex] = 1;
                        stack.push_back({static_cast<uint32_t>(nx), static_cast<uint32_t>(nz)});
                    }
                }

                auto inComponent = [&](const int32_t x, const int32_t z) -> bool {
                    if (x < minX || x > maxX || z < minZ || z > maxZ) {
                        return false;
                    }
                    return componentMask[static_cast<size_t>((z - minZ) * width + (x - minX))] != 0;
                };

                bool componentDrew = false;
                for (int32_t z = minZ; z <= maxZ; ++z) {
                    int32_t x = minX;
                    while (x <= maxX) {
                        if (!inComponent(x, z)) {
                            ++x;
                            continue;
                        }
                        const int32_t startX = x;
                        while (x <= maxX && inComponent(x, z)) {
                            ++x;
                        }
                        const int32_t endX = x - 1;
                        if (endX - startX + 1 >= 2) {
                            componentDrew |= drawRun({static_cast<uint32_t>(startX), static_cast<uint32_t>(z)},
                                                     {static_cast<uint32_t>(endX), static_cast<uint32_t>(z)});
                        }
                    }
                }

                for (int32_t x = minX; x <= maxX; ++x) {
                    int32_t z = minZ;
                    while (z <= maxZ) {
                        if (!inComponent(x, z)) {
                            ++z;
                            continue;
                        }
                        const int32_t startZ = z;
                        while (z <= maxZ && inComponent(x, z)) {
                            ++z;
                        }
                        const int32_t endZ = z - 1;
                        if (endZ - startZ + 1 >= 2) {
                            componentDrew |= drawRun({static_cast<uint32_t>(x), static_cast<uint32_t>(startZ)},
                                                     {static_cast<uint32_t>(x), static_cast<uint32_t>(endZ)});
                        }
                    }
                }

                int32_t bestDist = INT32_MAX;
                int32_t targetX = static_cast<int32_t>(cells.front().x);
                int32_t targetZ = static_cast<int32_t>(cells.front().y);
                int32_t intersectionCost = 0;
                bool placedIntersection = false;
                for (const auto& cell : cells) {
                    const int32_t x = static_cast<int32_t>(cell.x);
                    const int32_t z = static_cast<int32_t>(cell.y);
                    const int32_t dist = std::abs(x - anchorX) + std::abs(z - anchorZ);
                    if (dist > bestDist) {
                        continue;
                    }
                    int32_t previewCost = 0;
                    if (!ZoneDeveloperHooks::PlaceIntersectionByRuleId(city, x, z, intersectionRuleId, false, &previewCost)) {
                        continue;
                    }
                    bestDist = dist;
                    targetX = x;
                    targetZ = z;
                    intersectionCost = previewCost;
                    placedIntersection = true;
                }

                if (placedIntersection) {
                    placedIntersection = ZoneDeveloperHooks::PlaceIntersectionByRuleId(
                        city,
                        targetX,
                        targetZ,
                        intersectionRuleId,
                        true,
                        &intersectionCost);
                }

                LOG_INFO("ZoneViewInputControl: component={} cells={} drew={} intersection={} target={},{} cost={}",
                         componentCount,
                         static_cast<int32_t>(cells.size()),
                         componentDrew ? 1 : 0,
                         placedIntersection ? 1 : 0,
                         targetX,
                         targetZ,
                         intersectionCost);
                anyDrew |= componentDrew;
            }
        }

        LOG_INFO("ZoneViewInputControl: component override summary components={} drew_any={}",
                 componentCount,
                 anyDrew ? 1 : 0);
        return anyDrew;
    }

    bool RetrofitNewNetworkComponents(cISC4City* city,
                                      cISC4ZoneDeveloper* zoneDeveloper,
                                      const int32_t minX,
                                      const int32_t minZ,
                                      const int32_t maxX,
                                      const int32_t maxZ,
                                      const int32_t anchorX,
                                      const int32_t anchorZ,
                                      const std::vector<uint8_t>& preNetworkMap,
                                      const uint32_t intersectionRuleId)
    {
        if (!city || !zoneDeveloper || preNetworkMap.empty()) {
            return false;
        }

        const int32_t width = maxX - minX + 1;
        auto isNewNetworkCell = [&](const int32_t x, const int32_t z) -> bool {
            if (x < minX || x > maxX || z < minZ || z > maxZ) {
                return false;
            }
            const size_t index = static_cast<size_t>((z - minZ) * width + (x - minX));
            return preNetworkMap[index] == 0 && HasZoningNetworkAtCell(zoneDeveloper, x, z);
        };

        std::vector<uint8_t> visited(static_cast<size_t>((maxZ - minZ + 1) * width), 0);
        bool anyPlaced = false;
        int32_t componentCount = 0;

        for (int32_t seedZ = minZ; seedZ <= maxZ; ++seedZ) {
            for (int32_t seedX = minX; seedX <= maxX; ++seedX) {
                const size_t seedIndex = static_cast<size_t>((seedZ - minZ) * width + (seedX - minX));
                if (visited[seedIndex] || !isNewNetworkCell(seedX, seedZ)) {
                    continue;
                }

                ++componentCount;
                std::vector<SC4Point<uint32_t>> stack;
                std::vector<SC4Point<uint32_t>> cells;
                stack.push_back({static_cast<uint32_t>(seedX), static_cast<uint32_t>(seedZ)});
                visited[seedIndex] = 1;

                while (!stack.empty()) {
                    const SC4Point<uint32_t> point = stack.back();
                    stack.pop_back();
                    cells.push_back(point);
                    const int32_t x = static_cast<int32_t>(point.x);
                    const int32_t z = static_cast<int32_t>(point.y);

                    constexpr int32_t kDirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                    for (const auto& dir : kDirs) {
                        const int32_t nx = x + dir[0];
                        const int32_t nz = z + dir[1];
                        if (nx < minX || nx > maxX || nz < minZ || nz > maxZ) {
                            continue;
                        }
                        const size_t nIndex = static_cast<size_t>((nz - minZ) * width + (nx - minX));
                        if (visited[nIndex] || !isNewNetworkCell(nx, nz)) {
                            continue;
                        }
                        visited[nIndex] = 1;
                        stack.push_back({static_cast<uint32_t>(nx), static_cast<uint32_t>(nz)});
                    }
                }

                int32_t bestDist = INT32_MAX;
                int32_t targetX = static_cast<int32_t>(cells.front().x);
                int32_t targetZ = static_cast<int32_t>(cells.front().y);
                int32_t intersectionCost = 0;
                bool placedIntersection = false;
                for (const auto& cell : cells) {
                    const int32_t x = static_cast<int32_t>(cell.x);
                    const int32_t z = static_cast<int32_t>(cell.y);
                    const int32_t dist = std::abs(x - anchorX) + std::abs(z - anchorZ);
                    if (dist > bestDist) {
                        continue;
                    }
                    int32_t previewCost = 0;
                    if (!ZoneDeveloperHooks::PlaceIntersectionByRuleId(city, x, z, intersectionRuleId, false, &previewCost)) {
                        continue;
                    }
                    bestDist = dist;
                    targetX = x;
                    targetZ = z;
                    intersectionCost = previewCost;
                    placedIntersection = true;
                }

                if (placedIntersection) {
                    placedIntersection = ZoneDeveloperHooks::PlaceIntersectionByRuleId(
                        city,
                        targetX,
                        targetZ,
                        intersectionRuleId,
                        true,
                        &intersectionCost);
                }

                LOG_INFO("ZoneViewInputControl: retrofit component={} cells={} intersection={} target={},{} cost={}",
                         componentCount,
                         static_cast<int32_t>(cells.size()),
                         placedIntersection ? 1 : 0,
                         targetX,
                         targetZ,
                         intersectionCost);
                anyPlaced |= placedIntersection;
            }
        }

        QueueRetrofitForNewNetworkTiles(city, zoneDeveloper, minX, minZ, maxX, maxZ, preNetworkMap);
        LOG_INFO("ZoneViewInputControl: retrofit override summary components={} placed_any={}",
                 componentCount,
                 anyPlaced ? 1 : 0);
        return anyPlaced;
    }
}

ZoneViewInputControl::ZoneViewInputControl()
    : cSC4BaseViewInputControl(kZoneViewInputControlID)
    , isActive_(false)
    , dragging_(false)
    , startCellX_(0)
    , startCellZ_(0)
    , currentCellX_(0)
    , currentCellZ_(0)
    , onCancel_()
{
}

ZoneViewInputControl::~ZoneViewInputControl()
{
    LOG_INFO("ZoneViewInputControl destroyed");
}

bool ZoneViewInputControl::Init()
{
    if (!cSC4BaseViewInputControl::Init()) {
        return false;
    }

    LOG_INFO("ZoneViewInputControl initialized");
    return true;
}

bool ZoneViewInputControl::Shutdown()
{
    CancelDrag_();
    isActive_ = false;
    return cSC4BaseViewInputControl::Shutdown();
}

void ZoneViewInputControl::Activate()
{
    cSC4BaseViewInputControl::Activate();
    if (!Init()) {
        LOG_WARN("ZoneViewInputControl: Init failed during Activate");
        return;
    }

    isActive_ = true;
    UpdateCursorText_();
}

void ZoneViewInputControl::Deactivate()
{
    CancelDrag_();
    isActive_ = false;
    cSC4BaseViewInputControl::Deactivate();
}

bool ZoneViewInputControl::OnMouseDownL(const int32_t x, const int32_t z, const uint32_t)
{
    if (!isActive_ || !IsOnTop()) {
        return false;
    }

    int32_t cellX = 0;
    int32_t cellZ = 0;
    if (!PickCell_(x, z, cellX, cellZ)) {
        return false;
    }

    if (!SetCapture()) {
        return false;
    }

    dragging_ = true;
    startCellX_ = cellX;
    startCellZ_ = cellZ;
    currentCellX_ = cellX;
    currentCellZ_ = cellZ;
    UpdateCursorText_();
    UpdatePreview_();
    return true;
}

bool ZoneViewInputControl::OnMouseUpL(const int32_t x, const int32_t z, const uint32_t)
{
    if (!isActive_ || !dragging_) {
        return false;
    }

    int32_t cellX = currentCellX_;
    int32_t cellZ = currentCellZ_;
    PickCell_(x, z, cellX, cellZ);
    currentCellX_ = cellX;
    currentCellZ_ = cellZ;

    const bool committed = CommitSelection_();
    CancelDrag_();
    return committed;
}

bool ZoneViewInputControl::OnMouseMove(const int32_t x, const int32_t z, const uint32_t)
{
    if (!isActive_) {
        return false;
    }

    int32_t cellX = currentCellX_;
    int32_t cellZ = currentCellZ_;
    if (!PickCell_(x, z, cellX, cellZ)) {
        return false;
    }

    currentCellX_ = cellX;
    currentCellZ_ = cellZ;
    UpdateCursorText_();

    if (dragging_) {
        UpdatePreview_();
    }

    return true;
}

bool ZoneViewInputControl::OnMouseDownR(const int32_t, const int32_t, const uint32_t)
{
    if (!isActive_) {
        return false;
    }

    if (dragging_) {
        CancelDrag_();
        return true;
    }

    return false;
}

bool ZoneViewInputControl::OnMouseExit()
{
    if (!isActive_) {
        return false;
    }

    ClearCursorText_();
    return false;
}

bool ZoneViewInputControl::OnKeyDown(const int32_t vkCode, const uint32_t)
{
    if (!isActive_ || vkCode != VK_ESCAPE) {
        return false;
    }

    if (dragging_) {
        CancelDrag_();
    }
    else if (onCancel_) {
        onCancel_();
    }

    return true;
}

void ZoneViewInputControl::SetOnCancel(std::function<void()> onCancel)
{
    onCancel_ = std::move(onCancel);
}

bool ZoneViewInputControl::TryGetServices_(cISC4City*& city,
                                           cISC4ZoneManager*& zoneManager,
                                           cISC4ZoneDeveloper*& zoneDeveloper) const
{
    city = nullptr;
    zoneManager = nullptr;
    zoneDeveloper = nullptr;

    cISC4AppPtr app;
    if (!app) {
        return false;
    }

    city = app->GetCity();
    if (!city) {
        return false;
    }

    zoneManager = city->GetZoneManager();
    zoneDeveloper = city->GetZoneDeveloper();
    return zoneManager && zoneDeveloper;
}

bool ZoneViewInputControl::PickCell_(const int32_t screenX, const int32_t screenZ, int32_t& cellX, int32_t& cellZ) const
{
    if (!view3D) {
        return false;
    }

    cISC4AppPtr app;
    cISC4City* city = app ? app->GetCity() : nullptr;
    if (!city) {
        return false;
    }

    float world[3] = {0.0f, 0.0f, 0.0f};
    if (!view3D->PickTerrain(screenX, screenZ, world, false)) {
        return false;
    }

    int pickedCellX = 0;
    int pickedCellZ = 0;
    if (!city->PositionToCell(world[0], world[2], pickedCellX, pickedCellZ)) {
        return false;
    }

    if (!city->CellIsInBounds(pickedCellX, pickedCellZ)) {
        return false;
    }

    cellX = pickedCellX;
    cellZ = pickedCellZ;
    return true;
}

SC4CellRegion<long> ZoneViewInputControl::BuildDeveloperRegion_() const
{
    const long minX = std::min<long>(startCellX_, currentCellX_);
    const long minZ = std::min<long>(startCellZ_, currentCellZ_);
    const long maxX = std::max<long>(startCellX_, currentCellX_);
    const long maxZ = std::max<long>(startCellZ_, currentCellZ_);

    SC4CellRegion<long> region(minX, minZ, maxX, maxZ, false);
    for (long z = minZ; z <= maxZ; ++z) {
        for (long x = minX; x <= maxX; ++x) {
            region.cellMap.SetValue(static_cast<uint32_t>(x - minX),
                                    static_cast<uint32_t>(z - minZ),
                                    true);
        }
    }
    return region;
}

SC4CellRegion<int32_t> ZoneViewInputControl::BuildZoneManagerRegion_() const
{
    const int32_t minX = std::min(startCellX_, currentCellX_);
    const int32_t minZ = std::min(startCellZ_, currentCellZ_);
    const int32_t maxX = std::max(startCellX_, currentCellX_);
    const int32_t maxZ = std::max(startCellZ_, currentCellZ_);

    SC4CellRegion<int32_t> region(minX, minZ, maxX, maxZ, false);
    for (int32_t z = minZ; z <= maxZ; ++z) {
        for (int32_t x = minX; x <= maxX; ++x) {
            region.cellMap.SetValue(static_cast<uint32_t>(x - minX),
                                    static_cast<uint32_t>(z - minZ),
                                    true);
        }
    }
    return region;
}

void ZoneViewInputControl::UpdateCursorText_()
{
    if (!view3D || !isActive_) {
        return;
    }

    const ZoneTuningSettings& settings = GetZoneTuningSettings();
    char bodyBuffer[96] = {};
    std::snprintf(bodyBuffer,
                  sizeof(bodyBuffer),
                  "Network Tool: %d (%s)",
                  settings.experimentalNetworkToolType,
                  GetExperimentalNetworkToolLabel(settings.experimentalNetworkToolType));

    cRZBaseString title(GetZoneTypeLabel(settings.zoneType));
    cRZBaseString body(bodyBuffer);
    view3D->SetCursorText(kCursorTextId, kCursorTextPriority, &title, &body, 0);
}

void ZoneViewInputControl::ClearCursorText_()
{
    if (view3D) {
        view3D->ClearCursorText(kCursorTextId);
    }
}

bool ZoneViewInputControl::UpdatePreview_()
{
    const ZoneTuningSettings& settings = GetZoneTuningSettings();
    if (!settings.showLivePreview || settings.zoneType == cISC4ZoneManager::ZoneType::Plopped) {
        return false;
    }

    cISC4City* city = nullptr;
    cISC4ZoneManager* zoneManager = nullptr;
    cISC4ZoneDeveloper* zoneDeveloper = nullptr;
    if (!TryGetServices_(city, zoneManager, zoneDeveloper)) {
        return false;
    }
    (void)city;
    (void)zoneManager;

    zoneDeveloper->SetOptions(settings.alternateOrientation,
                              settings.allowInternalStreets,
                              settings.customZoneSize);

    SC4CellRegion<long> region = BuildDeveloperRegion_();
    SC4Point<long> focusPoint{static_cast<long>(currentCellX_), static_cast<long>(currentCellZ_)};
    zoneDeveloper->HighlightParcels(region, settings.zoneType, &focusPoint, nullptr);
    return true;
}

bool ZoneViewInputControl::CommitSelection_()
{
    const ZoneTuningSettings& settings = GetZoneTuningSettings();
    if (settings.zoneType == cISC4ZoneManager::ZoneType::Plopped) {
        if (view3D) {
            view3D->SetErrorReportString("Plopped zones are not supported by this sample tool.");
        }
        return false;
    }

    cISC4City* city = nullptr;
    cISC4ZoneManager* zoneManager = nullptr;
    cISC4ZoneDeveloper* zoneDeveloper = nullptr;
    if (!TryGetServices_(city, zoneManager, zoneDeveloper)) {
        return false;
    }
    (void)city;

    zoneDeveloper->SetOptions(settings.alternateOrientation,
                              settings.allowInternalStreets,
                              settings.customZoneSize);

    SC4CellRegion<int32_t> zoneRegion = BuildZoneManagerRegion_();
    SC4CellRegion<long> developerRegion = BuildDeveloperRegion_();
    const int32_t minX = std::min(startCellX_, currentCellX_);
    const int32_t minZ = std::min(startCellZ_, currentCellZ_);
    const int32_t maxX = std::max(startCellX_, currentCellX_);
    const int32_t maxZ = std::max(startCellZ_, currentCellZ_);
    const int32_t anchorX = currentCellX_;
    const int32_t anchorZ = currentCellZ_;

    std::vector<uint8_t> preNetworkMap;
    if (settings.placeIntersectionOnCommit) {
        const int32_t width = maxX - minX + 1;
        const int32_t height = maxZ - minZ + 1;
        preNetworkMap.resize(static_cast<size_t>(width * height), 0);
        for (int32_t z = minZ; z <= maxZ; ++z) {
            for (int32_t x = minX; x <= maxX; ++x) {
                const size_t index = static_cast<size_t>((z - minZ) * width + (x - minX));
                preNetworkMap[index] = HasZoningNetworkAtCell(zoneDeveloper, x, z) ? 1u : 0u;
            }
        }
    }

    int64_t zonedCellCount = 0;
    int32_t errorCode = 0;
    const bool placeZone = settings.zoneType != cISC4ZoneManager::ZoneType::None;
    const bool placed = zoneManager->PlaceZone(zoneRegion,
                                               settings.zoneType,
                                               placeZone,
                                               settings.skipFundsCheck,
                                               true,
                                               settings.customZoneSize,
                                               true,
                                               &zonedCellCount,
                                               &errorCode,
                                               0);

    if (!placed) {
        if (view3D) {
            char errorBuffer[96] = {};
            std::snprintf(errorBuffer, sizeof(errorBuffer), "Zone placement failed (error %d).", errorCode);
            view3D->SetErrorReportString(errorBuffer);
        }
        return false;
    }

    zoneDeveloper->DoParcellization(developerRegion, settings.zoneType, settings.allowNetworkCleanup);

    if (settings.placeIntersectionOnCommit) {
        bool overrideResult = false;
        switch (settings.networkOverrideMode) {
            case ZoneNetworkOverrideMode::DrawSegments:
                overrideResult = OverrideNewNetworkComponents(city,
                                                              zoneDeveloper,
                                                              minX,
                                                              minZ,
                                                              maxX,
                                                              maxZ,
                                                              anchorX,
                                                              anchorZ,
                                                              preNetworkMap,
                                                              settings.experimentalNetworkToolType,
                                                              settings.zoneVicIntersectionRuleId);
                LOG_INFO("ZoneViewInputControl: draw-segments override result={}", overrideResult ? 1 : 0);
                break;
            case ZoneNetworkOverrideMode::Retrofit:
                overrideResult = RetrofitNewNetworkComponents(city,
                                                              zoneDeveloper,
                                                              minX,
                                                              minZ,
                                                              maxX,
                                                              maxZ,
                                                              anchorX,
                                                              anchorZ,
                                                              preNetworkMap,
                                                              settings.zoneVicIntersectionRuleId);
                LOG_INFO("ZoneViewInputControl: retrofit override result={}", overrideResult ? 1 : 0);
                break;
        }
    }

    if (view3D && zonedCellCount == 0) {
        view3D->SetErrorReportString("Zone operation completed, but no cells changed.");
    }
    return true;
}

void ZoneViewInputControl::ClearPreview_()
{
    cISC4City* city = nullptr;
    cISC4ZoneManager* zoneManager = nullptr;
    cISC4ZoneDeveloper* zoneDeveloper = nullptr;
    if (TryGetServices_(city, zoneManager, zoneDeveloper)) {
        ZoneDeveloperHooks::ClearLiveHighlight(zoneDeveloper);
    }

    ClearCursorText_();
}

void ZoneViewInputControl::CancelDrag_()
{
    dragging_ = false;
    ReleaseCapture();
    ClearPreview_();
    UpdateCursorText_();
}

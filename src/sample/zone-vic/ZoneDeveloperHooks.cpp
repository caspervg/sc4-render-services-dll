#include "sample/zone-vic/ZoneDeveloperHooks.hpp"

#include "cISC4City.h"
#include "cISC4NetworkManager.h"
#include "sample/zone-vic/ZoneTuningSettings.hpp"
#include "cISC4NetworkTool.h"
#include "utils/Logger.h"
#include "utils/VersionDetection.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <new>

namespace
{
    constexpr uint16_t kSupportedGameVersion = 641;
    constexpr uintptr_t kGetIntersectionRuleAddress = 0x00625380;
    constexpr uintptr_t kDoAutoCompleteAddress = 0x006097E0;
    constexpr uintptr_t kInsertIsolatedHighwayIntersectionAddress = 0x0062CD50;
    constexpr uintptr_t kNetworkToolCtorAddress = 0x0062C5F0;
    constexpr size_t kNetworkToolSize = 0x368;
    constexpr uintptr_t kDetermineLotSizeAddress = 0x00732BF0;
    constexpr uintptr_t kExtendAsStreetsAddress = 0x00730BA0;
    constexpr uintptr_t kLayInJogStreetsAddress = 0x00730D30;
    constexpr uintptr_t kDrawStreetAddress = 0x0072A2A0;
    constexpr uintptr_t kClearHighlightAddress = 0x0072C0E0;
    constexpr size_t kHookByteCount = 5;

    enum class PatchKind : uint8_t
    {
        DetermineLotSize,
        ExtendAsStreets,
        LayInJogStreets,
        DrawStreet,
    };

    struct CallSitePatch
    {
        const char* name;
        PatchKind kind;
        uintptr_t callSiteAddress;
        uintptr_t expectedOriginalTarget;
        void* hookFn;
        bool installed;
        int32_t originalRel;
        uintptr_t originalTarget;
    };

    using DetermineLotSizeFn = void(__thiscall*)(void*, void*);
    using GetIntersectionRuleFn = void*(__cdecl*)(uint32_t);
    using DoAutoCompleteFn = void(__thiscall*)(void*, int32_t, int32_t, cISC4NetworkTool*);
    using InsertIsolatedHighwayIntersectionFn = bool(__thiscall*)(cISC4NetworkTool*, int32_t, int32_t, uint32_t, bool);
    using NetworkToolCtorFn = void*(__thiscall*)(void*, int32_t);
    using StreetHelperFn = void(__thiscall*)(void*, void*, int32_t);
    using DrawStreetFn = void(__thiscall*)(void*, int32_t, int32_t, int32_t, int32_t, char);
    using ClearHighlightFn = void(__thiscall*)(void*);

    void __fastcall DetermineLotSizeHook(void* self, void*, void* region);
    void __fastcall ExtendAsStreetsHook(void* self, void*, void* region, int32_t networkType);
    void __fastcall LayInJogStreetsHook(void* self, void*, void* region, int32_t networkType);
    void __fastcall DrawStreetHook(void* self, void*, int32_t x1, int32_t z1, int32_t x2, int32_t z2, char fillEndpoints);

    std::mutex gPatchMutex;
    bool gInstalled = false;

    std::array<CallSitePatch, 20> gCallSitePatches{{
        {"ZoneDeveloper::DetermineLotSize_", PatchKind::DetermineLotSize, 0x00733954, kDetermineLotSizeAddress, reinterpret_cast<void*>(&DetermineLotSizeHook), false, 0, 0},
        {"ZoneDeveloper::ExtendAsStreets_ [0x0B]", PatchKind::ExtendAsStreets, 0x00733968, kExtendAsStreetsAddress, reinterpret_cast<void*>(&ExtendAsStreetsHook), false, 0, 0},
        {"ZoneDeveloper::ExtendAsStreets_ [0x03]", PatchKind::ExtendAsStreets, 0x00733972, kExtendAsStreetsAddress, reinterpret_cast<void*>(&ExtendAsStreetsHook), false, 0, 0},
        {"ZoneDeveloper::ExtendAsStreets_ [0x0A]", PatchKind::ExtendAsStreets, 0x0073397C, kExtendAsStreetsAddress, reinterpret_cast<void*>(&ExtendAsStreetsHook), false, 0, 0},
        {"ZoneDeveloper::ExtendAsStreets_ [0x00]", PatchKind::ExtendAsStreets, 0x00733986, kExtendAsStreetsAddress, reinterpret_cast<void*>(&ExtendAsStreetsHook), false, 0, 0},
        {"ZoneDeveloper::ExtendAsStreets_ [0x06]", PatchKind::ExtendAsStreets, 0x00733990, kExtendAsStreetsAddress, reinterpret_cast<void*>(&ExtendAsStreetsHook), false, 0, 0},
        {"ZoneDeveloper::LayInJogStreets_ [0x0B]", PatchKind::LayInJogStreets, 0x007339B1, kLayInJogStreetsAddress, reinterpret_cast<void*>(&LayInJogStreetsHook), false, 0, 0},
        {"ZoneDeveloper::LayInJogStreets_ [0x03]", PatchKind::LayInJogStreets, 0x007339BB, kLayInJogStreetsAddress, reinterpret_cast<void*>(&LayInJogStreetsHook), false, 0, 0},
        {"ZoneDeveloper::LayInJogStreets_ [0x0A]", PatchKind::LayInJogStreets, 0x007339C5, kLayInJogStreetsAddress, reinterpret_cast<void*>(&LayInJogStreetsHook), false, 0, 0},
        {"ZoneDeveloper::LayInJogStreets_ [0x00]", PatchKind::LayInJogStreets, 0x007339CF, kLayInJogStreetsAddress, reinterpret_cast<void*>(&LayInJogStreetsHook), false, 0, 0},
        {"ZoneDeveloper::LayInJogStreets_ [0x06]", PatchKind::LayInJogStreets, 0x007339D9, kLayInJogStreetsAddress, reinterpret_cast<void*>(&LayInJogStreetsHook), false, 0, 0},
        {"ZoneDeveloper::SimpleParcellize_::DrawStreet [A]", PatchKind::DrawStreet, 0x0072EAA6, kDrawStreetAddress, reinterpret_cast<void*>(&DrawStreetHook), false, 0, 0},
        {"ZoneDeveloper::SimpleParcellize_::DrawStreet [B]", PatchKind::DrawStreet, 0x0072EAFA, kDrawStreetAddress, reinterpret_cast<void*>(&DrawStreetHook), false, 0, 0},
        {"ZoneDeveloper::Parcellize_::DrawStreet [A]", PatchKind::DrawStreet, 0x0072F4D3, kDrawStreetAddress, reinterpret_cast<void*>(&DrawStreetHook), false, 0, 0},
        {"ZoneDeveloper::Parcellize_::DrawStreet [B]", PatchKind::DrawStreet, 0x0072F6FA, kDrawStreetAddress, reinterpret_cast<void*>(&DrawStreetHook), false, 0, 0},
        {"ZoneDeveloper::ExtendAsStreets_::DrawStreet", PatchKind::DrawStreet, 0x00730CCE, kDrawStreetAddress, reinterpret_cast<void*>(&DrawStreetHook), false, 0, 0},
        {"ZoneDeveloper::LayInJogStreets_::DrawStreet [A]", PatchKind::DrawStreet, 0x00731103, kDrawStreetAddress, reinterpret_cast<void*>(&DrawStreetHook), false, 0, 0},
        {"ZoneDeveloper::LayInJogStreets_::DrawStreet [B]", PatchKind::DrawStreet, 0x007314D5, kDrawStreetAddress, reinterpret_cast<void*>(&DrawStreetHook), false, 0, 0},
        {"ZoneDeveloper::LayInJogStreets_::DrawStreet [C]", PatchKind::DrawStreet, 0x0073189A, kDrawStreetAddress, reinterpret_cast<void*>(&DrawStreetHook), false, 0, 0},
        {"ZoneDeveloper::LayInJogStreets_::DrawStreet [D]", PatchKind::DrawStreet, 0x00731C6F, kDrawStreetAddress, reinterpret_cast<void*>(&DrawStreetHook), false, 0, 0},
    }};

    bool ComputeRelativeCallTarget(const uintptr_t callSiteAddress, const uintptr_t targetAddress, int32_t& relOut)
    {
        const auto delta = static_cast<intptr_t>(targetAddress) - static_cast<intptr_t>(callSiteAddress + kHookByteCount);
        if (delta < static_cast<intptr_t>(INT32_MIN) || delta > static_cast<intptr_t>(INT32_MAX)) {
            return false;
        }

        relOut = static_cast<int32_t>(delta);
        return true;
    }

    template<typename T>
    T& FieldAt(void* base, const size_t offset)
    {
        return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(base) + offset);
    }

    int32_t ClampLotMetric(const int32_t value)
    {
        return std::clamp(value, 1, 32);
    }

    int32_t ClampStreetSpacing(const int32_t value)
    {
        return std::clamp(value, 1, 64);
    }

    int32_t GetPreferredLotWidth()
    {
        return ClampLotMetric(GetZoneTuningSettings().preferredLotWidth);
    }

    int32_t GetPreferredLotDepth()
    {
        return ClampLotMetric(GetZoneTuningSettings().preferredLotDepth);
    }

    bool MatchesFrontagePreference(const ZoneFrontagePreference preference, const int32_t networkType)
    {
        switch (preference) {
            case ZoneFrontagePreference::Default:
                return true;
            case ZoneFrontagePreference::Road:
                return networkType == 0;
            case ZoneFrontagePreference::Street:
                return networkType == 0x0B;
            case ZoneFrontagePreference::Avenue:
                return true;
            case ZoneFrontagePreference::Rail:
                return networkType == 3;
            case ZoneFrontagePreference::GroundHighway:
                return networkType == 10;
            case ZoneFrontagePreference::Highway:
                return true;
            case ZoneFrontagePreference::Subway:
                return networkType == 6;
            case ZoneFrontagePreference::ElevatedRail:
                return true;
            case ZoneFrontagePreference::OneWayRoad:
                return true;
            case ZoneFrontagePreference::Monorail:
                return true;
            case ZoneFrontagePreference::ANT:
                return true;
        }

        return true;
    }

    void AdjustDetermineLotSizeResult(void* zoneDeveloper)
    {
        ZoneTuningSettings& settings = GetZoneTuningSettings();
        if (!settings.toolEnabled || !settings.hookOverridesEnabled || !zoneDeveloper) {
            return;
        }

        int32_t& lotWidth = FieldAt<int32_t>(zoneDeveloper, 0x84);
        int32_t& lotHeight = FieldAt<int32_t>(zoneDeveloper, 0x88);
        int32_t& minWidth = FieldAt<int32_t>(zoneDeveloper, 0x8C);
        int32_t& minHeight = FieldAt<int32_t>(zoneDeveloper, 0x90);
        int32_t& streetInterval = FieldAt<int32_t>(zoneDeveloper, 0x94);

        switch (settings.parcelGoal) {
            case ZoneParcelGoal::Balanced:
                return;
            case ZoneParcelGoal::SmallLots:
                lotWidth = ClampLotMetric(lotWidth - 1);
                lotHeight = ClampLotMetric(lotHeight - 1);
                minWidth = ClampLotMetric(std::min(minWidth, lotWidth));
                minHeight = ClampLotMetric(std::min(minHeight, lotHeight));
                streetInterval = ClampStreetSpacing(streetInterval - 1);
                break;
            case ZoneParcelGoal::LargeLots:
                lotWidth = ClampLotMetric(lotWidth + 2);
                lotHeight = ClampLotMetric(lotHeight + 2);
                minWidth = ClampLotMetric(std::max(minWidth, lotWidth - 1));
                minHeight = ClampLotMetric(std::max(minHeight, lotHeight - 1));
                streetInterval = ClampStreetSpacing(streetInterval + 2);
                break;
            case ZoneParcelGoal::MaxFrontage:
                lotWidth = ClampLotMetric(std::min(lotWidth, 2));
                lotHeight = ClampLotMetric(std::max(lotHeight, 5));
                minWidth = ClampLotMetric(std::min(minWidth, lotWidth));
                minHeight = ClampLotMetric(std::max(minHeight, lotHeight - 1));
                streetInterval = ClampStreetSpacing(streetInterval);
                break;
            case ZoneParcelGoal::MinStreets:
                lotWidth = ClampLotMetric(lotWidth + 1);
                lotHeight = ClampLotMetric(lotHeight + 1);
                minWidth = ClampLotMetric(std::max(minWidth, lotWidth - 1));
                minHeight = ClampLotMetric(std::max(minHeight, lotHeight - 1));
                streetInterval = ClampStreetSpacing(streetInterval + 4);
                break;
            case ZoneParcelGoal::PreserveExisting:
                streetInterval = ClampStreetSpacing(streetInterval + 1);
                break;
            case ZoneParcelGoal::PreferLotSize:
            {
                const int32_t preferredWidth = GetPreferredLotWidth();
                const int32_t preferredDepth = GetPreferredLotDepth();
                lotWidth = preferredWidth;
                lotHeight = preferredDepth;
                minWidth = preferredWidth;
                minHeight = preferredDepth;
                streetInterval = ClampStreetSpacing(std::max(streetInterval, std::max(preferredWidth, preferredDepth) + 2));
                break;
            }
        }
    }

    bool ShouldSkipStreetHelper(const bool jogHelper, const int32_t networkType)
    {
        const ZoneTuningSettings& settings = GetZoneTuningSettings();
        if (!settings.toolEnabled || !settings.hookOverridesEnabled) {
            return false;
        }

        if (!settings.allowInternalStreets) {
            return true;
        }

        switch (settings.streetPolicy) {
            case ZoneStreetPolicy::Vanilla:
            case ZoneStreetPolicy::Dense:
                break;
            case ZoneStreetPolicy::None:
                return true;
            case ZoneStreetPolicy::Minimal:
                if (jogHelper) {
                    return true;
                }
                break;
        }

        return !MatchesFrontagePreference(settings.frontagePreference, networkType);
    }

    uintptr_t VTableOf(const uintptr_t objectAddress)
    {
        if (objectAddress == 0) {
            return 0;
        }

        return *reinterpret_cast<uintptr_t*>(objectAddress);
    }

    uintptr_t ResolveBackendPointerOverride(const ZoneDeveloperBackendSource source,
                                            const uintptr_t backend44,
                                            const uintptr_t backend48,
                                            const uintptr_t backend4C,
                                            const uintptr_t backend50,
                                            const uintptr_t currentValue)
    {
        switch (source) {
            case ZoneDeveloperBackendSource::Default:
                return currentValue;
            case ZoneDeveloperBackendSource::Null:
                return 0;
            case ZoneDeveloperBackendSource::Backend44:
                return backend44;
            case ZoneDeveloperBackendSource::Backend48:
                return backend48;
            case ZoneDeveloperBackendSource::Backend4C:
                return backend4C;
            case ZoneDeveloperBackendSource::Backend50:
                return backend50;
        }

        return currentValue;
    }

    bool InstallCallSitePatch(CallSitePatch& patch)
    {
        if (patch.installed) {
            return true;
        }

        auto* site = reinterpret_cast<uint8_t*>(patch.callSiteAddress);
        if (site[0] != 0xE8) {
            LOG_ERROR("ZoneDeveloperHooks: expected CALL rel32 at 0x{:08X} for {}",
                      static_cast<uint32_t>(patch.callSiteAddress),
                      patch.name);
            return false;
        }

        std::memcpy(&patch.originalRel, site + 1, sizeof(patch.originalRel));
        patch.originalTarget = patch.callSiteAddress + kHookByteCount + patch.originalRel;
        if (patch.expectedOriginalTarget != 0 && patch.originalTarget != patch.expectedOriginalTarget) {
            LOG_ERROR("ZoneDeveloperHooks: {} original target mismatch, got 0x{:08X} expected 0x{:08X}",
                      patch.name,
                      static_cast<uint32_t>(patch.originalTarget),
                      static_cast<uint32_t>(patch.expectedOriginalTarget));
            patch.originalTarget = 0;
            patch.originalRel = 0;
            return false;
        }

        int32_t newRel = 0;
        if (!ComputeRelativeCallTarget(patch.callSiteAddress, reinterpret_cast<uintptr_t>(patch.hookFn), newRel)) {
            LOG_ERROR("ZoneDeveloperHooks: rel32 range failure for {}", patch.name);
            return false;
        }

        DWORD oldProtect = 0;
        if (!VirtualProtect(site + 1, sizeof(newRel), PAGE_EXECUTE_READWRITE, &oldProtect)) {
            LOG_ERROR("ZoneDeveloperHooks: VirtualProtect failed for {}", patch.name);
            return false;
        }

        std::memcpy(site + 1, &newRel, sizeof(newRel));
        FlushInstructionCache(GetCurrentProcess(), site, kHookByteCount);
        VirtualProtect(site + 1, sizeof(newRel), oldProtect, &oldProtect);

        patch.installed = true;
        return true;
    }

    void UninstallCallSitePatch(CallSitePatch& patch)
    {
        if (!patch.installed) {
            return;
        }

        auto* site = reinterpret_cast<uint8_t*>(patch.callSiteAddress);
        DWORD oldProtect = 0;
        if (!VirtualProtect(site + 1, sizeof(patch.originalRel), PAGE_EXECUTE_READWRITE, &oldProtect)) {
            LOG_ERROR("ZoneDeveloperHooks: VirtualProtect failed while uninstalling {}", patch.name);
            return;
        }

        std::memcpy(site + 1, &patch.originalRel, sizeof(patch.originalRel));
        FlushInstructionCache(GetCurrentProcess(), site, kHookByteCount);
        VirtualProtect(site + 1, sizeof(patch.originalRel), oldProtect, &oldProtect);

        patch.installed = false;
        patch.originalRel = 0;
        patch.originalTarget = 0;
    }

    void __fastcall DetermineLotSizeHook(void* self, void*, void* region)
    {
        const auto original = reinterpret_cast<DetermineLotSizeFn>(kDetermineLotSizeAddress);
        original(self, region);
        AdjustDetermineLotSizeResult(self);
    }

    void __fastcall ExtendAsStreetsHook(void* self, void*, void* region, const int32_t networkType)
    {
        if (ShouldSkipStreetHelper(false, networkType)) {
            return;
        }

        const auto original = reinterpret_cast<StreetHelperFn>(kExtendAsStreetsAddress);
        original(self, region, networkType);
    }

    void __fastcall LayInJogStreetsHook(void* self, void*, void* region, const int32_t networkType)
    {
        if (ShouldSkipStreetHelper(true, networkType)) {
            return;
        }

        const auto original = reinterpret_cast<StreetHelperFn>(kLayInJogStreetsAddress);
        original(self, region, networkType);
    }

    void __fastcall DrawStreetHook(void* self, void*, const int32_t x1, const int32_t z1,
                                   const int32_t x2, const int32_t z2, const char fillEndpoints)
    {
        const auto original = reinterpret_cast<DrawStreetFn>(kDrawStreetAddress);
        original(self, x1, z1, x2, z2, fillEndpoints);
    }
}

bool ZoneDeveloperHooks::SupportsCurrentVersion()
{
    return VersionDetection::GetInstance().GetGameVersion() == kSupportedGameVersion;
}

bool ZoneDeveloperHooks::Install()
{
    std::lock_guard<std::mutex> lock(gPatchMutex);
    if (gInstalled) {
        return true;
    }

    if (!SupportsCurrentVersion()) {
        LOG_WARN("ZoneDeveloperHooks: private hooks disabled for game version {}",
                 VersionDetection::GetInstance().GetGameVersion());
        return false;
    }

    for (auto& patch : gCallSitePatches) {
        if (!InstallCallSitePatch(patch)) {
            for (auto& rollbackPatch : gCallSitePatches) {
                UninstallCallSitePatch(rollbackPatch);
            }
            return false;
        }
    }

    gInstalled = true;
    LOG_INFO("ZoneDeveloperHooks: installed {} callsite patches", gCallSitePatches.size());
    return true;
}

void ZoneDeveloperHooks::Uninstall()
{
    std::lock_guard<std::mutex> lock(gPatchMutex);
    for (auto& patch : gCallSitePatches) {
        UninstallCallSitePatch(patch);
    }
    gInstalled = false;
}

bool ZoneDeveloperHooks::IsInstalled()
{
    std::lock_guard<std::mutex> lock(gPatchMutex);
    return gInstalled;
}

bool ZoneDeveloperHooks::ClearLiveHighlight(cISC4ZoneDeveloper* zoneDeveloper)
{
    if (!zoneDeveloper || !SupportsCurrentVersion()) {
        return false;
    }

    const auto clearHighlight = reinterpret_cast<ClearHighlightFn>(kClearHighlightAddress);
    clearHighlight(zoneDeveloper);
    return true;
}

bool ZoneDeveloperHooks::ReadDebugState(cISC4ZoneDeveloper* zoneDeveloper, ZoneDeveloperDebugState& state)
{
    state = {};
    if (!zoneDeveloper || !SupportsCurrentVersion()) {
        return false;
    }

    auto* self = reinterpret_cast<uint8_t*>(zoneDeveloper);
    state.available = true;
    state.self = reinterpret_cast<uintptr_t>(zoneDeveloper);
    state.backend44 = FieldAt<uintptr_t>(self, 0x44);
    state.backend48 = FieldAt<uintptr_t>(self, 0x48);
    state.backend4C = FieldAt<uintptr_t>(self, 0x4C);
    state.backend50 = FieldAt<uintptr_t>(self, 0x50);
    state.backend44VTable = VTableOf(state.backend44);
    state.backend48VTable = VTableOf(state.backend48);
    state.backend4CVTable = VTableOf(state.backend4C);
    state.backend50VTable = VTableOf(state.backend50);
    state.lotWidth = FieldAt<int32_t>(self, 0x84);
    state.lotHeight = FieldAt<int32_t>(self, 0x88);
    state.minWidth = FieldAt<int32_t>(self, 0x8C);
    state.minHeight = FieldAt<int32_t>(self, 0x90);
    state.streetInterval = FieldAt<int32_t>(self, 0x94);
    state.flag98 = FieldAt<uint8_t>(self, 0x98);
    state.flag99 = FieldAt<uint8_t>(self, 0x99);
    state.flag9A = FieldAt<uint8_t>(self, 0x9A);
    state.flag9B = FieldAt<uint8_t>(self, 0x9B);
    state.flag9C = FieldAt<uint8_t>(self, 0x9C);
    state.zoneTypeRaw = FieldAt<int32_t>(self, 0xA0);
    state.allowedRegion = FieldAt<int32_t>(self, 0xA4);
    state.focusPoint = FieldAt<int32_t>(self, 0xA8);
    state.orientationPrimary = FieldAt<int32_t>(self, 0xCC);
    state.orientationSecondary = FieldAt<int32_t>(self, 0xD0);
    return true;
}

bool ZoneDeveloperHooks::ApplyDebugPokes(cISC4ZoneDeveloper* zoneDeveloper)
{
    (void)zoneDeveloper;
    return true;
}

bool ZoneDeveloperHooks::ReadNetworkToolDebugState(cISC4NetworkTool* networkTool, NetworkToolDebugState& state)
{
    state = {};
    if (!networkTool || !SupportsCurrentVersion()) {
        return false;
    }

    auto* self = reinterpret_cast<uint8_t*>(networkTool);
    state.available = true;
    state.self = reinterpret_cast<uintptr_t>(networkTool);
    state.vtable = VTableOf(state.self);
    state.toolType = FieldAt<int32_t>(self, 0x35C);
    state.blockedMask = FieldAt<uint32_t>(self, 0x1EC);
    state.mode50 = FieldAt<uint8_t>(self, 0x50);
    state.flag52 = FieldAt<uint8_t>(self, 0x52);
    state.flag53 = FieldAt<uint8_t>(self, 0x53);
    return true;
}

bool ZoneDeveloperHooks::CreateFreshNetworkTool(const int32_t networkType, cISC4NetworkTool*& outTool)
{
    if (!SupportsCurrentVersion()) {
        return false;
    }

    DestroyFreshNetworkTool(outTool);

    void* rawMemory = ::operator new(kNetworkToolSize, std::nothrow);
    if (!rawMemory) {
        return false;
    }

    const auto ctor = reinterpret_cast<NetworkToolCtorFn>(kNetworkToolCtorAddress);
    auto* tool = reinterpret_cast<cISC4NetworkTool*>(ctor(rawMemory, networkType));
    if (!tool) {
        ::operator delete(rawMemory);
        return false;
    }

    tool->AddRef();
    outTool = tool;
    return true;
}

bool ZoneDeveloperHooks::InitFreshNetworkTool(cISC4NetworkTool* tool)
{
    if (!tool) {
        return false;
    }

    return tool->Init();
}

void ZoneDeveloperHooks::ResetFreshNetworkTool(cISC4NetworkTool* tool)
{
    if (tool) {
        tool->Reset();
    }
}

void ZoneDeveloperHooks::DestroyFreshNetworkTool(cISC4NetworkTool*& tool)
{
    if (tool) {
        tool->Release();
    }
    tool = nullptr;
}

bool ZoneDeveloperHooks::PlaceIntersectionByRuleId(cISC4City* city,
                                                   const int32_t cellX,
                                                   const int32_t cellZ,
                                                   const uint32_t ruleId,
                                                   const bool commit,
                                                   int32_t* outCost)
{
    LOG_INFO("ZoneDeveloperHooks: PlaceIntersectionByRuleId enter city=0x{:08X} x={} z={} rule=0x{:08X} commit={}",
             reinterpret_cast<uintptr_t>(city),
             cellX,
             cellZ,
             ruleId,
             commit ? 1 : 0);

    if (!SupportsCurrentVersion() || !city) {
        LOG_WARN("ZoneDeveloperHooks: PlaceIntersectionByRuleId early exit, supported={} city=0x{:08X}",
                 SupportsCurrentVersion() ? 1 : 0,
                 reinterpret_cast<uintptr_t>(city));
        return false;
    }

    LOG_INFO("ZoneDeveloperHooks: requesting city->GetNetworkManager()");
    cISC4NetworkManager* networkManager = city->GetNetworkManager();
    LOG_INFO("ZoneDeveloperHooks: GetNetworkManager -> 0x{:08X}",
             reinterpret_cast<uintptr_t>(networkManager));
    if (!networkManager) {
        LOG_WARN("ZoneDeveloperHooks: PlaceIntersectionByRuleId no network manager");
        return false;
    }

    LOG_INFO("ZoneDeveloperHooks: requesting networkManager->GetNetworkTool(type=2, unique=0)");
    cISC4NetworkTool* tool = networkManager->GetNetworkTool(2, false);
    LOG_INFO("ZoneDeveloperHooks: GetNetworkTool -> 0x{:08X} vtbl=0x{:08X}",
             reinterpret_cast<uintptr_t>(tool),
             tool ? *reinterpret_cast<uintptr_t*>(tool) : 0);
    if (!tool) {
        LOG_WARN("ZoneDeveloperHooks: PlaceIntersectionByRuleId no network tool");
        return false;
    }

    LOG_INFO("ZoneDeveloperHooks: calling tool->Init()");
    tool->Init();
    LOG_INFO("ZoneDeveloperHooks: tool->Init() returned");

    LOG_INFO("ZoneDeveloperHooks: calling tool->Reset()");
    tool->Reset();
    LOG_INFO("ZoneDeveloperHooks: tool->Reset() returned");

    const auto getIntersectionRule = reinterpret_cast<GetIntersectionRuleFn>(kGetIntersectionRuleAddress);
    LOG_INFO("ZoneDeveloperHooks: calling GetIntersectionRule(0x{:08X}) at 0x{:08X}",
             ruleId,
             static_cast<uint32_t>(kGetIntersectionRuleAddress));
    void* rule = getIntersectionRule(ruleId);
    LOG_INFO("ZoneDeveloperHooks: GetIntersectionRule -> 0x{:08X}",
             reinterpret_cast<uintptr_t>(rule));
    if (!rule) {
        LOG_WARN("ZoneDeveloperHooks: PlaceIntersectionByRuleId missing rule 0x{:08X}", ruleId);
        LOG_INFO("ZoneDeveloperHooks: releasing network tool after missing rule");
        tool->Release();
        return false;
    }

    const auto doAutoComplete = reinterpret_cast<DoAutoCompleteFn>(kDoAutoCompleteAddress);
    LOG_INFO("ZoneDeveloperHooks: calling DoAutoComplete(rule=0x{:08X}, x={}, z={}, tool=0x{:08X}) at 0x{:08X}",
             reinterpret_cast<uintptr_t>(rule),
             cellX,
             cellZ,
             reinterpret_cast<uintptr_t>(tool),
             static_cast<uint32_t>(kDoAutoCompleteAddress));
    doAutoComplete(rule, cellX, cellZ, tool);
    LOG_INFO("ZoneDeveloperHooks: DoAutoComplete returned");

    const auto insertIntersection =
        reinterpret_cast<InsertIsolatedHighwayIntersectionFn>(kInsertIsolatedHighwayIntersectionAddress);
    LOG_INFO("ZoneDeveloperHooks: calling InsertIsolatedHighwayIntersection(tool=0x{:08X}, x={}, z={}, rule=0x{:08X}, commit={}) at 0x{:08X}",
             reinterpret_cast<uintptr_t>(tool),
             cellX,
             cellZ,
             ruleId,
             commit ? 1 : 0,
             static_cast<uint32_t>(kInsertIsolatedHighwayIntersectionAddress));
    const bool ok = insertIntersection(tool, cellX, cellZ, ruleId, commit);
    LOG_INFO("ZoneDeveloperHooks: InsertIsolatedHighwayIntersection returned {}",
             ok ? 1 : 0);

    if (outCost) {
        LOG_INFO("ZoneDeveloperHooks: querying tool->GetCostOfSolution()");
        *outCost = tool->GetCostOfSolution();
        LOG_INFO("ZoneDeveloperHooks: GetCostOfSolution -> {}", *outCost);
    }

    //LOG_INFO("ZoneDeveloperHooks: releasing network tool 0x{:08X}",
    //         reinterpret_cast<uintptr_t>(tool));
    //tool->Release();
    LOG_INFO("ZoneDeveloperHooks: PlaceIntersectionByRuleId exit success={}", ok ? 1 : 0);
    return ok;
}

bool ZoneDeveloperHooks::ApplyNetworkManagerToolOverrides(cISC4ZoneDeveloper* zoneDeveloper,
                                                          cISC4NetworkManager* networkManager)
{
    (void)zoneDeveloper;
    (void)networkManager;
    return true;
}

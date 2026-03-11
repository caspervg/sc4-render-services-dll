#pragma once

#include "cISC4ZoneManager.h"

#include <cstdint>

enum class ZoneParcelGoal : uint8_t
{
    Balanced = 0,
    SmallLots = 1,
    LargeLots = 2,
    MaxFrontage = 3,
    MinStreets = 4,
    PreserveExisting = 5,
    PreferLotSize = 6,
};

enum class ZoneFrontagePreference : uint8_t
{
    Default = 0,
    Road = 1,
    Street = 2,
    Avenue = 3,
    Rail = 4,
    GroundHighway = 5,
    Highway = 6,
    Subway = 7,
    ElevatedRail = 8,
    OneWayRoad = 9,
    Monorail = 10,
    ANT = 11,
};

enum class ZoneStreetPolicy : uint8_t
{
    Vanilla = 0,
    None = 1,
    Minimal = 2,
    Dense = 3,
};

enum class ZoneInternalNetworkPreference : uint8_t
{
    Default = 0,
    Road = 1,
    Street = 2,
    Avenue = 3,
    Rail = 4,
    GroundHighway = 5,
    Highway = 6,
    Subway = 7,
    ElevatedRail = 8,
    OneWayRoad = 9,
    Monorail = 10,
    ANT = 11,
};

enum class ZoneDeveloperBackendSource : uint8_t
{
    Default = 0,
    Null = 1,
    Backend44 = 2,
    Backend48 = 3,
    Backend4C = 4,
    Backend50 = 5,
};

enum class ZoneNetworkOverrideMode : uint8_t
{
    DrawSegments = 0,
    Retrofit = 1,
};

struct ZoneTuningSettings
{
    bool toolEnabled = false;
    bool hookOverridesEnabled = true;
    bool showLivePreview = true;
    bool skipFundsCheck = true;
    bool allowNetworkCleanup = true;
    bool alternateOrientation = false;
    bool allowInternalStreets = true;
    bool customZoneSize = false;
    bool placeIntersectionOnCommit = false;
    int32_t preferredLotWidth = 3;
    int32_t preferredLotDepth = 3;
    int32_t experimentalNetworkToolType = 3;
    uint32_t zoneVicIntersectionRuleId = 0x00006700;
    int32_t intersectionCellX = 0;
    int32_t intersectionCellZ = 0;
    uint32_t intersectionRuleId = 0;

    cISC4ZoneManager::ZoneType zoneType = cISC4ZoneManager::ZoneType::ResidentialLowDensity;
    ZoneParcelGoal parcelGoal = ZoneParcelGoal::Balanced;
    ZoneFrontagePreference frontagePreference = ZoneFrontagePreference::Default;
    ZoneStreetPolicy streetPolicy = ZoneStreetPolicy::Vanilla;
    ZoneInternalNetworkPreference internalNetworkPreference = ZoneInternalNetworkPreference::Default;
    ZoneNetworkOverrideMode networkOverrideMode = ZoneNetworkOverrideMode::DrawSegments;
};

ZoneTuningSettings& GetZoneTuningSettings();

const char* GetZoneTypeLabel(cISC4ZoneManager::ZoneType type);
const char* GetParcelGoalLabel(ZoneParcelGoal goal);
const char* GetFrontagePreferenceLabel(ZoneFrontagePreference preference);
const char* GetStreetPolicyLabel(ZoneStreetPolicy policy);
const char* GetInternalNetworkPreferenceLabel(ZoneInternalNetworkPreference preference);
const char* GetZoneDeveloperBackendSourceLabel(ZoneDeveloperBackendSource source);
const char* GetExperimentalNetworkToolLabel(int32_t toolType);
const char* GetNetworkOverrideModeLabel(ZoneNetworkOverrideMode mode);

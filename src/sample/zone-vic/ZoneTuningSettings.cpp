#include "sample/zone-vic/ZoneTuningSettings.hpp"

namespace
{
    ZoneTuningSettings gSettings{};
}

ZoneTuningSettings& GetZoneTuningSettings()
{
    return gSettings;
}

const char* GetZoneTypeLabel(const cISC4ZoneManager::ZoneType type)
{
    using ZoneType = cISC4ZoneManager::ZoneType;

    switch (type) {
        case ZoneType::None:
            return "None / Dezone";
        case ZoneType::ResidentialLowDensity:
            return "Residential Low";
        case ZoneType::ResidentialMediumDensity:
            return "Residential Medium";
        case ZoneType::ResidentialHighDensity:
            return "Residential High";
        case ZoneType::CommercialLowDensity:
            return "Commercial Low";
        case ZoneType::CommercialMediumDensity:
            return "Commercial Medium";
        case ZoneType::CommercialHighDensity:
            return "Commercial High";
        case ZoneType::Agriculture:
            return "Agriculture";
        case ZoneType::IndustrialMediumDensity:
            return "Industrial Medium";
        case ZoneType::IndustrialHighDensity:
            return "Industrial High";
        case ZoneType::Military:
            return "Military";
        case ZoneType::Airport:
            return "Airport";
        case ZoneType::Seaport:
            return "Seaport";
        case ZoneType::Spaceport:
            return "Spaceport";
        case ZoneType::Landfill:
            return "Landfill";
        case ZoneType::Plopped:
            return "Plopped (Unsupported)";
    }

    return "Unknown";
}

const char* GetParcelGoalLabel(const ZoneParcelGoal goal)
{
    switch (goal) {
        case ZoneParcelGoal::Balanced:
            return "Balanced";
        case ZoneParcelGoal::SmallLots:
            return "Small Lots";
        case ZoneParcelGoal::LargeLots:
            return "Large Lots";
        case ZoneParcelGoal::MaxFrontage:
            return "Max Frontage";
        case ZoneParcelGoal::MinStreets:
            return "Min Streets";
        case ZoneParcelGoal::PreserveExisting:
            return "Preserve Existing";
        case ZoneParcelGoal::PreferLotSize:
            return "Prefer Lot Size";
    }

    return "Unknown";
}

const char* GetFrontagePreferenceLabel(const ZoneFrontagePreference preference)
{
    switch (preference) {
        case ZoneFrontagePreference::Default:
            return "Default";
        case ZoneFrontagePreference::Road:
            return "Road";
        case ZoneFrontagePreference::Street:
            return "Street";
        case ZoneFrontagePreference::Avenue:
            return "Avenue";
        case ZoneFrontagePreference::Rail:
            return "Rail";
        case ZoneFrontagePreference::GroundHighway:
            return "Ground Highway";
        case ZoneFrontagePreference::Highway:
            return "Highway";
        case ZoneFrontagePreference::Subway:
            return "Subway";
        case ZoneFrontagePreference::ElevatedRail:
            return "Elevated Rail";
        case ZoneFrontagePreference::OneWayRoad:
            return "One-Way Road";
        case ZoneFrontagePreference::Monorail:
            return "Monorail";
        case ZoneFrontagePreference::ANT:
            return "ANT (RHW)";
    }

    return "Unknown";
}

const char* GetStreetPolicyLabel(const ZoneStreetPolicy policy)
{
    switch (policy) {
        case ZoneStreetPolicy::Vanilla:
            return "Vanilla";
        case ZoneStreetPolicy::None:
            return "None";
        case ZoneStreetPolicy::Minimal:
            return "Minimal";
        case ZoneStreetPolicy::Dense:
            return "Dense";
    }

    return "Unknown";
}

const char* GetInternalNetworkPreferenceLabel(const ZoneInternalNetworkPreference preference)
{
    switch (preference) {
        case ZoneInternalNetworkPreference::Default:
            return "Default";
        case ZoneInternalNetworkPreference::Road:
            return "Road";
        case ZoneInternalNetworkPreference::Street:
            return "Street";
        case ZoneInternalNetworkPreference::Avenue:
            return "Avenue";
        case ZoneInternalNetworkPreference::Rail:
            return "Rail";
        case ZoneInternalNetworkPreference::GroundHighway:
            return "Ground Highway";
        case ZoneInternalNetworkPreference::Highway:
            return "Highway";
        case ZoneInternalNetworkPreference::Subway:
            return "Subway";
        case ZoneInternalNetworkPreference::ElevatedRail:
            return "Elevated Rail";
        case ZoneInternalNetworkPreference::OneWayRoad:
            return "One-Way Road";
        case ZoneInternalNetworkPreference::Monorail:
            return "Monorail";
        case ZoneInternalNetworkPreference::ANT:
            return "ANT (RHW)";
    }

    return "Unknown";
}

const char* GetZoneDeveloperBackendSourceLabel(const ZoneDeveloperBackendSource source)
{
    switch (source) {
        case ZoneDeveloperBackendSource::Default:
            return "Default";
        case ZoneDeveloperBackendSource::Null:
            return "Null";
        case ZoneDeveloperBackendSource::Backend44:
            return "+0x44";
        case ZoneDeveloperBackendSource::Backend48:
            return "+0x48";
        case ZoneDeveloperBackendSource::Backend4C:
            return "+0x4C";
        case ZoneDeveloperBackendSource::Backend50:
            return "+0x50";
    }

    return "Unknown";
}

const char* GetExperimentalNetworkToolLabel(const int32_t toolType)
{
    switch (toolType) {
        case 0:
            return "Road";
        case 1:
            return "Rail";
        case 2:
            return "Elevated Highway";
        case 3:
            return "Street";
        case 4:
            return "Pipe Tool";
        case 5:
            return "PowerLine Tool";
        case 6:
            return "Avenue";
        case 7:
            return "Subway Tool";
        case 8:
            return "Elevated Rail";
        case 9:
            return "Monorail";
        case 10:
            return "One-Way Road";
        case 11:
            return "ANT / RHW";
        case 12:
            return "Ground Highway";
        default:
            return "Unknown";
    }
}

const char* GetNetworkOverrideModeLabel(const ZoneNetworkOverrideMode mode)
{
    switch (mode) {
        case ZoneNetworkOverrideMode::DrawSegments:
            return "DrawNetworkLine / PlaceAllSegments";
        case ZoneNetworkOverrideMode::Retrofit:
            return "NetworkLot + Retrofit";
    }

    return "Unknown";
}

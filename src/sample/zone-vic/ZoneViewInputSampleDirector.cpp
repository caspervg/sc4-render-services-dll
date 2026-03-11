#include "cRZCOMDllDirector.h"

#include "cIGZFrameWork.h"
#include "GZServPtrs.h"
#include "SC4UI.h"
#include "cISC4App.h"
#include "cISC4City.h"
#include "cISC4NetworkTool.h"
#include "cISC4ZoneDeveloper.h"
#include "imgui.h"
#include "public/ImGuiPanelAdapter.h"
#include "public/ImGuiServiceIds.h"
#include "public/cIGZImGuiService.h"
#include "sample/zone-vic/ZoneDeveloperHooks.hpp"
#include "sample/zone-vic/ZoneTuningSettings.hpp"
#include "sample/zone-vic/ZoneViewInputControl.hpp"
#include "utils/Logger.h"
#include "utils/VersionDetection.h"

#include <array>
#include <atomic>

namespace
{
    constexpr uint32_t kZoneViewInputDirectorID = 0x57C44A12;
    constexpr uint32_t kZoneViewInputPanelId = 0x57C44A13;

    ZoneViewInputControl* gZoneInputTool = nullptr;
    std::atomic<bool> gZoneToolEnabled{false};
    cISC4NetworkTool* gExperimentalTool48 = nullptr;

    constexpr std::array<cISC4ZoneManager::ZoneType, 16> kZoneTypes{{
        cISC4ZoneManager::ZoneType::None,
        cISC4ZoneManager::ZoneType::ResidentialLowDensity,
        cISC4ZoneManager::ZoneType::ResidentialMediumDensity,
        cISC4ZoneManager::ZoneType::ResidentialHighDensity,
        cISC4ZoneManager::ZoneType::CommercialLowDensity,
        cISC4ZoneManager::ZoneType::CommercialMediumDensity,
        cISC4ZoneManager::ZoneType::CommercialHighDensity,
        cISC4ZoneManager::ZoneType::Agriculture,
        cISC4ZoneManager::ZoneType::IndustrialMediumDensity,
        cISC4ZoneManager::ZoneType::IndustrialHighDensity,
        cISC4ZoneManager::ZoneType::Military,
        cISC4ZoneManager::ZoneType::Airport,
        cISC4ZoneManager::ZoneType::Seaport,
        cISC4ZoneManager::ZoneType::Spaceport,
        cISC4ZoneManager::ZoneType::Landfill,
        cISC4ZoneManager::ZoneType::Plopped,
    }};

    constexpr std::array<ZoneParcelGoal, 7> kParcelGoals{{
        ZoneParcelGoal::Balanced,
        ZoneParcelGoal::SmallLots,
        ZoneParcelGoal::LargeLots,
        ZoneParcelGoal::MaxFrontage,
        ZoneParcelGoal::MinStreets,
        ZoneParcelGoal::PreserveExisting,
        ZoneParcelGoal::PreferLotSize,
    }};

    constexpr std::array<ZoneFrontagePreference, 12> kFrontagePreferences{{
        ZoneFrontagePreference::Default,
        ZoneFrontagePreference::Road,
        ZoneFrontagePreference::Street,
        ZoneFrontagePreference::Avenue,
        ZoneFrontagePreference::Rail,
        ZoneFrontagePreference::GroundHighway,
        ZoneFrontagePreference::Highway,
        ZoneFrontagePreference::Subway,
        ZoneFrontagePreference::ElevatedRail,
        ZoneFrontagePreference::OneWayRoad,
        ZoneFrontagePreference::Monorail,
        ZoneFrontagePreference::ANT,
    }};

    constexpr std::array<ZoneStreetPolicy, 4> kStreetPolicies{{
        ZoneStreetPolicy::Vanilla,
        ZoneStreetPolicy::None,
        ZoneStreetPolicy::Minimal,
        ZoneStreetPolicy::Dense,
    }};

    constexpr std::array<ZoneInternalNetworkPreference, 12> kInternalNetworkPreferences{{
        ZoneInternalNetworkPreference::Default,
        ZoneInternalNetworkPreference::Road,
        ZoneInternalNetworkPreference::Street,
        ZoneInternalNetworkPreference::Avenue,
        ZoneInternalNetworkPreference::Rail,
        ZoneInternalNetworkPreference::GroundHighway,
        ZoneInternalNetworkPreference::Highway,
        ZoneInternalNetworkPreference::Subway,
        ZoneInternalNetworkPreference::ElevatedRail,
        ZoneInternalNetworkPreference::OneWayRoad,
        ZoneInternalNetworkPreference::Monorail,
        ZoneInternalNetworkPreference::ANT,
    }};

    constexpr std::array<ZoneNetworkOverrideMode, 2> kNetworkOverrideModes{{
        ZoneNetworkOverrideMode::DrawSegments,
        ZoneNetworkOverrideMode::Retrofit,
    }};

    struct NetworkToolChoice
    {
        int32_t type;
        const char* label;
    };

    constexpr std::array<NetworkToolChoice, 13> kExperimentalNetworkToolTypes{{
        {0, "0 - Road"},
        {1, "1 - Rail"},
        {2, "2 - Elevated Highway"},
        {3, "3 - Street"},
        {4, "4 - Pipe Tool"},
        {5, "5 - PowerLine Tool"},
        {6, "6 - Avenue"},
        {7, "7 - Subway Tool"},
        {8, "8 - Elevated Rail"},
        {9, "9 - Monorail"},
        {10, "10 - One-Way Road"},
        {11, "11 - ANT / RHW"},
        {12, "12 - Ground Highway"},
    }};

    bool IsCityViewAvailable()
    {
        const cISC4AppPtr app;
        return app && app->GetCity() != nullptr && SC4UI::GetView3DWin() != nullptr;
    }

    template<typename T, size_t N>
    bool DrawEnumCombo(const char* label, T& currentValue, const std::array<T, N>& values, const char* (*labelFn)(T))
    {
        bool changed = false;
        if (ImGui::BeginCombo(label, labelFn(currentValue))) {
            for (const T value : values) {
                const bool selected = value == currentValue;
                if (ImGui::Selectable(labelFn(value), selected)) {
                    currentValue = value;
                    changed = true;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    bool DrawNetworkToolChoiceCombo(const char* label, int32_t& currentValue)
    {
        const auto currentIt = std::find_if(
            kExperimentalNetworkToolTypes.begin(),
            kExperimentalNetworkToolTypes.end(),
            [&](const NetworkToolChoice& choice) { return choice.type == currentValue; });
        const char* preview = currentIt != kExperimentalNetworkToolTypes.end() ? currentIt->label : "Unknown";

        bool changed = false;
        if (ImGui::BeginCombo(label, preview)) {
            for (const auto& choice : kExperimentalNetworkToolTypes) {
                const bool selected = choice.type == currentValue;
                if (ImGui::Selectable(choice.label, selected)) {
                    currentValue = choice.type;
                    changed = true;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    void DisableZoneTool();

    bool EnableZoneTool()
    {
        if (gZoneToolEnabled.load(std::memory_order_relaxed)) {
            return true;
        }

        if (!IsCityViewAvailable()) {
            LOG_WARN("ZoneViewInput: city view not available");
            return false;
        }

        cISC4View3DWin* view3D = SC4UI::GetView3DWin();
        if (!view3D) {
            return false;
        }

        if (!gZoneInputTool) {
            gZoneInputTool = new ZoneViewInputControl();
            gZoneInputTool->AddRef();
            gZoneInputTool->SetOnCancel([]() { DisableZoneTool(); });
            gZoneInputTool->Activate();
        }

        if (!view3D->SetCurrentViewInputControl(gZoneInputTool,
                                                cISC4View3DWin::ViewInputControlStackOperation_RemoveCurrentControl)) {
            LOG_WARN("ZoneViewInput: failed to set current view input control");
            return false;
        }

        gZoneToolEnabled.store(true, std::memory_order_relaxed);
        GetZoneTuningSettings().toolEnabled = true;
        return true;
    }

    void DisableZoneTool()
    {
        if (!gZoneToolEnabled.load(std::memory_order_relaxed)) {
            GetZoneTuningSettings().toolEnabled = false;
            return;
        }

        if (auto view3D = SC4UI::GetView3DWin()) {
            auto* currentControl = view3D->GetCurrentViewInputControl();
            if (currentControl == gZoneInputTool) {
                view3D->RemoveCurrentViewInputControl(false);
            }
        }

        gZoneToolEnabled.store(false, std::memory_order_relaxed);
        GetZoneTuningSettings().toolEnabled = false;
    }

    void DestroyZoneTool()
    {
        DisableZoneTool();
        if (gZoneInputTool) {
            gZoneInputTool->Release();
            gZoneInputTool = nullptr;
        }
    }

    void DestroyExperimentalTool()
    {
        ZoneDeveloperHooks::DestroyFreshNetworkTool(gExperimentalTool48);
    }

    class ZoneViewInputPanel final : public ImGuiPanel
    {
    public:
        void OnShutdown() override
        {
            delete this;
        }

        void OnRender() override
        {
            ZoneTuningSettings& settings = GetZoneTuningSettings();

            ImGui::Begin("Zone Tool Tuning", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

            bool enabled = gZoneToolEnabled.load(std::memory_order_relaxed);
            if (ImGui::Checkbox("Enable Tool", &enabled)) {
                if (enabled) {
                    enabled = EnableZoneTool();
                }
                else {
                    DisableZoneTool();
                }
                gZoneToolEnabled.store(enabled, std::memory_order_relaxed);
                settings.toolEnabled = enabled;
            }

            ImGui::Separator();
            ImGui::Text("Game version: %u", VersionDetection::GetInstance().GetGameVersion());
            if (ZoneDeveloperHooks::SupportsCurrentVersion()) {
                ImGui::TextUnformatted(ZoneDeveloperHooks::IsInstalled()
                    ? "Private ZoneDeveloper hooks active."
                    : "Private ZoneDeveloper hooks unavailable.");
            }
            else {
                ImGui::TextUnformatted("Public-only mode. Private hooks are disabled on this game version.");
            }

            DrawEnumCombo("Zone Type", settings.zoneType, kZoneTypes, &GetZoneTypeLabel);
            DrawEnumCombo("Parcel Goal", settings.parcelGoal, kParcelGoals, &GetParcelGoalLabel);
            DrawEnumCombo("Frontage Preference", settings.frontagePreference, kFrontagePreferences, &GetFrontagePreferenceLabel);
            DrawEnumCombo("Street Policy", settings.streetPolicy, kStreetPolicies, &GetStreetPolicyLabel);
            DrawEnumCombo("Internal Network", settings.internalNetworkPreference, kInternalNetworkPreferences, &GetInternalNetworkPreferenceLabel);
            DrawEnumCombo("Network Override Mode", settings.networkOverrideMode, kNetworkOverrideModes, &GetNetworkOverrideModeLabel);

            if (settings.parcelGoal == ZoneParcelGoal::PreferLotSize) {
                int preferredWidth = settings.preferredLotWidth;
                int preferredDepth = settings.preferredLotDepth;
                if (ImGui::SliderInt("Target Lot Width", &preferredWidth, 1, 12)) {
                    settings.preferredLotWidth = preferredWidth;
                }
                if (ImGui::SliderInt("Target Lot Depth", &preferredDepth, 1, 12)) {
                    settings.preferredLotDepth = preferredDepth;
                }
            }

            ImGui::Separator();
            ImGui::Checkbox("Live Preview", &settings.showLivePreview);
            ImGui::Checkbox("Hook Overrides", &settings.hookOverridesEnabled);
            ImGui::Checkbox("Skip Funds Check", &settings.skipFundsCheck);
            ImGui::Checkbox("Allow Network Cleanup", &settings.allowNetworkCleanup);
            ImGui::Checkbox("Alternate Orientation", &settings.alternateOrientation);
            ImGui::Checkbox("Allow Internal Streets", &settings.allowInternalStreets);
            ImGui::Checkbox("Custom Zone Size", &settings.customZoneSize);
            ImGui::Checkbox("Intersection Override On Commit", &settings.placeIntersectionOnCommit);
            if (settings.placeIntersectionOnCommit) {
                ImGui::InputScalar("Zone VIC Intersection Rule",
                                   ImGuiDataType_U32,
                                   &settings.zoneVicIntersectionRuleId,
                                   nullptr,
                                   nullptr,
                                   "%08X");
            }

            ImGui::Separator();
            cISC4ZoneDeveloper* zoneDeveloper = nullptr;
            cISC4AppPtr app;
            if (app) {
                if (cISC4City* city = app->GetCity()) {
                    zoneDeveloper = city->GetZoneDeveloper();
                }
            }

            ZoneDeveloperDebugState debugState{};
            const bool haveDebugState = ZoneDeveloperHooks::ReadDebugState(zoneDeveloper, debugState);
            if (haveDebugState) {
                ImGui::Text("Live lot fields: %d x %d | min %d x %d | street %d",
                            debugState.lotWidth,
                            debugState.lotHeight,
                            debugState.minWidth,
                            debugState.minHeight,
                            debugState.streetInterval);

                NetworkToolDebugState backend48State{};
                if (ZoneDeveloperHooks::ReadNetworkToolDebugState(
                        reinterpret_cast<cISC4NetworkTool*>(debugState.backend48), backend48State)) {
                    ImGui::Text("Live +0x48 backend: ptr 0x%08X | tool %d | mask 0x%08X",
                                static_cast<uint32_t>(backend48State.self),
                                backend48State.toolType,
                                backend48State.blockedMask);
                }
            }

            DrawNetworkToolChoiceCombo("Experimental Network Tool", settings.experimentalNetworkToolType);

            if (ImGui::Button("Assign Fresh Tool To +0x48") && zoneDeveloper) {
                DestroyExperimentalTool();
                if (ZoneDeveloperHooks::CreateFreshNetworkTool(settings.experimentalNetworkToolType, gExperimentalTool48)) {
                    ZoneDeveloperHooks::InitFreshNetworkTool(gExperimentalTool48);
                    ZoneDeveloperHooks::ResetFreshNetworkTool(gExperimentalTool48);
                    auto* self = reinterpret_cast<uint8_t*>(zoneDeveloper);
                    *reinterpret_cast<uintptr_t*>(self + 0x48) = reinterpret_cast<uintptr_t>(gExperimentalTool48);
                }
            }

            if (ImGui::Button("Destroy Fresh +0x48 Tool")) {
                DestroyExperimentalTool();
            }

            NetworkToolDebugState experimentalState{};
            if (ZoneDeveloperHooks::ReadNetworkToolDebugState(gExperimentalTool48, experimentalState)) {
                ImGui::Text("Fresh +0x48 tool: ptr 0x%08X | tool %d | mask 0x%08X",
                            static_cast<uint32_t>(experimentalState.self),
                            experimentalState.toolType,
                            experimentalState.blockedMask);
            }

            ImGui::Separator();
            ImGui::TextUnformatted("Place Intersection By Rule ID");
            ImGui::InputInt("Cell X", &settings.intersectionCellX);
            ImGui::InputInt("Cell Z", &settings.intersectionCellZ);
            ImGui::InputScalar("Rule ID", ImGuiDataType_U32, &settings.intersectionRuleId, nullptr, nullptr, "%08X");

            static bool sLastIntersectionCallValid = false;
            static bool sLastIntersectionCallCommitted = false;
            static bool sLastIntersectionCallSucceeded = false;
            static int32_t sLastIntersectionCost = 0;

            if (ImGui::Button("Preview Intersection")) {
                sLastIntersectionCallValid = false;
                if (app) {
                    if (cISC4City* city = app->GetCity()) {
                        sLastIntersectionCallSucceeded = ZoneDeveloperHooks::PlaceIntersectionByRuleId(
                            city,
                            settings.intersectionCellX,
                            settings.intersectionCellZ,
                            settings.intersectionRuleId,
                            false,
                            &sLastIntersectionCost);
                        sLastIntersectionCallCommitted = false;
                        sLastIntersectionCallValid = true;
                    }
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Place Intersection")) {
                sLastIntersectionCallValid = false;
                if (app) {
                    if (cISC4City* city = app->GetCity()) {
                        sLastIntersectionCallSucceeded = ZoneDeveloperHooks::PlaceIntersectionByRuleId(
                            city,
                            settings.intersectionCellX,
                            settings.intersectionCellZ,
                            settings.intersectionRuleId,
                            true,
                            &sLastIntersectionCost);
                        sLastIntersectionCallCommitted = true;
                        sLastIntersectionCallValid = true;
                    }
                }
            }

            if (sLastIntersectionCallValid) {
                ImGui::Text("%s %s | cost %d",
                            sLastIntersectionCallCommitted ? "Place" : "Preview",
                            sLastIntersectionCallSucceeded ? "succeeded" : "failed",
                            sLastIntersectionCost);
            }

            ImGui::Separator();
            ImGui::TextUnformatted("Controls");
            ImGui::TextUnformatted("LMB drag: preview and place");
            ImGui::TextUnformatted("RMB: cancel current selection");
            ImGui::TextUnformatted("Esc: disable tool");

            ImGui::End();
        }
    };
}

class ZoneViewInputSampleDirector final : public cRZCOMDllDirector
{
public:
    [[nodiscard]] uint32_t GetDirectorID() const override
    {
        return kZoneViewInputDirectorID;
    }

    bool OnStart(cIGZCOM* pCOM) override
    {
        cRZCOMDllDirector::OnStart(pCOM);
        Logger::Initialize("SC4ZoneViewInputSample", "");
        if (mpFrameWork) {
            mpFrameWork->AddHook(this);
        }
        return true;
    }

    bool PostAppInit() override
    {
        if (!mpFrameWork || panelRegistered_) {
            return true;
        }

        ZoneDeveloperHooks::Install();

        if (!mpFrameWork->GetSystemService(kImGuiServiceID,
                                           GZIID_cIGZImGuiService,
                                           reinterpret_cast<void**>(&imguiService_))) {
            LOG_WARN("ZoneViewInput: ImGui service not available");
            return true;
        }

        auto* panel = new ZoneViewInputPanel();
        const ImGuiPanelDesc desc = ImGuiPanelAdapter<ZoneViewInputPanel>::MakeDesc(panel, kZoneViewInputPanelId, 135, true);
        if (!imguiService_->RegisterPanel(desc)) {
            delete panel;
            imguiService_->Release();
            imguiService_ = nullptr;
            return true;
        }

        panelRegistered_ = true;
        return true;
    }

    bool PostAppShutdown() override
    {
        DestroyZoneTool();
        DestroyExperimentalTool();
        ZoneDeveloperHooks::Uninstall();

        if (imguiService_) {
            imguiService_->UnregisterPanel(kZoneViewInputPanelId);
            imguiService_->Release();
            imguiService_ = nullptr;
        }

        panelRegistered_ = false;
        return true;
    }

private:
    cIGZImGuiService* imguiService_ = nullptr;
    bool panelRegistered_ = false;
};

static ZoneViewInputSampleDirector sDirector;

cRZCOMDllDirector* RZGetCOMDllDirector()
{
    static bool sAddedRef = false;
    if (!sAddedRef) {
        sDirector.AddRef();
        sAddedRef = true;
    }
    return &sDirector;
}

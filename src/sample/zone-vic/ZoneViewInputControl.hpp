#pragma once

#include "cSC4BaseViewInputControl.h"

#include <cstdint>
#include <functional>

class cISC4City;
class cISC4ZoneDeveloper;
class cISC4ZoneManager;
template<typename T> class SC4CellRegion;

class ZoneViewInputControl final : public cSC4BaseViewInputControl
{
public:
    static constexpr uint32_t kZoneViewInputControlID = 0x57C44A10;

    ZoneViewInputControl();
    ~ZoneViewInputControl() override;

    bool Init() override;
    bool Shutdown() override;

    void Activate() override;
    void Deactivate() override;

    bool OnMouseDownL(int32_t x, int32_t z, uint32_t modifiers) override;
    bool OnMouseUpL(int32_t x, int32_t z, uint32_t modifiers) override;
    bool OnMouseMove(int32_t x, int32_t z, uint32_t modifiers) override;
    bool OnMouseDownR(int32_t x, int32_t z, uint32_t modifiers) override;
    bool OnMouseExit() override;
    bool OnKeyDown(int32_t vkCode, uint32_t modifiers) override;

    void SetOnCancel(std::function<void()> onCancel);

private:
    bool TryGetServices_(cISC4City*& city, cISC4ZoneManager*& zoneManager, cISC4ZoneDeveloper*& zoneDeveloper) const;
    bool PickCell_(int32_t screenX, int32_t screenZ, int32_t& cellX, int32_t& cellZ) const;

    SC4CellRegion<long> BuildDeveloperRegion_() const;
    SC4CellRegion<int32_t> BuildZoneManagerRegion_() const;

    void UpdateCursorText_();
    void ClearCursorText_();
    bool UpdatePreview_();
    bool CommitSelection_();
    void ClearPreview_();
    void CancelDrag_();

private:
    bool isActive_;
    bool dragging_;
    int32_t startCellX_;
    int32_t startCellZ_;
    int32_t currentCellX_;
    int32_t currentCellZ_;
    std::function<void()> onCancel_;
};

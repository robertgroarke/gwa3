#include <gwa3/managers/MerchantMgr.h>
bool CraftConsumableViaUiClick(const char* targetLabel, uint32_t targetModelId, uint32_t targetItemId,
                               ConsumableHarnessClickMode clickMode,
                               uint32_t& beforeCount, uint32_t& afterCount,
                                char* detail, size_t detailSize) {
    if (detail && detailSize) detail[0] = '\0';

    const ConsumableCraftUiFrames frames =
        ResolveConsumableCraftUiFrames(targetLabel, targetModelId, targetItemId, clickMode);
    const uintptr_t merchantFrame = frames.merchantFrame;
    const uint32_t merchantItemPosition = frames.merchantItemPosition;
    const uintptr_t itemRowFrame = frames.itemRowFrame;
    const uintptr_t rowClickFrame = frames.rowClickFrame;
    const uintptr_t pathActionFrame = frames.pathActionFrame;
    const uintptr_t actionPrimaryByContext = frames.actionPrimaryByContext;
    const uintptr_t actionAltByContext = frames.actionAltByContext;
    const uintptr_t craftButtonFrame = frames.craftButtonFrame;

    // Avoid broad frame-dump traversal during live craft runs; it has been a
    // recurring crash source while the harness is trying to reach the row click.

    ConsumableMaterialCounter materialsBefore[7]{};
    FillConsumableMaterialCounters(materialsBefore);
    beforeCount = CountInventoryModelQuantity(targetModelId);

    char materialDetail[512] = {};
    FormatConsumableMaterialSnapshot(materialDetail, sizeof(materialDetail), materialsBefore, materialsBefore);
    WriteConsumableHarnessStatus("material_snapshot_before", targetLabel, ReadMapId(), 0,
                                 MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, beforeCount, 1, materialDetail);

    const bool rowClicked = TryClickConsumableCraftRow(frames, targetLabel, targetModelId, targetItemId,
                                                       clickMode, beforeCount, detail, detailSize);
    if (!rowClicked) {
        if (TryRecoverConsumableCraftAfterRowClickFailure(frames, targetLabel, targetModelId, targetItemId,
                                                         clickMode, beforeCount, afterCount, detail, detailSize)) {
            return true;
        }
        return false;
    }

    if (IsConsumableCraftRowOnlyMode(clickMode)) {
        afterCount = CountInventoryModelQuantity(targetModelId);
        if (detail && detailSize) {
            sprintf_s(detail, detailSize,
                      "ui rowClicked=%u craftClicked=0 mode=%s frame=0x%08X row=0x%08X rowClick=0x%08X itemPos=%u actionPath=0x%08X action125=0x%08X action126=0x%08X before=%u after=%u",
                      rowClicked ? 1u : 0u,
                      DescribeConsumableHarnessClickMode(clickMode),
                      static_cast<unsigned>(merchantFrame),
                      static_cast<unsigned>(itemRowFrame),
                      static_cast<unsigned>(rowClickFrame),
                      merchantItemPosition == UINT32_MAX ? 0xFFFFFFFFu : merchantItemPosition,
                      static_cast<unsigned>(pathActionFrame),
                      static_cast<unsigned>(actionPrimaryByContext),
                      static_cast<unsigned>(actionAltByContext),
                      beforeCount,
                      afterCount);
        }
        return rowClicked;
    }

    if (TryClickConsumableCraftButton(targetLabel, targetModelId, targetItemId,
                                      craftButtonFrame, beforeCount, afterCount,
                                      detail, detailSize)) {
        return true;
    }

    const bool craftClicked = TryConsumableCraftActionFallbacks(
        frames, targetLabel, targetModelId, targetItemId,
        clickMode, beforeCount, materialsBefore, materialDetail,
        detail, detailSize);

    afterCount = CountInventoryModelQuantity(targetModelId);
    if (detail && detailSize) {
        sprintf_s(detail, detailSize,
                  "ui rowClicked=%u craftClicked=%u frame=0x%08X row=0x%08X rowClick=0x%08X itemPos=%u actionPath=0x%08X action125=0x%08X action126=0x%08X craftButton=0x%08X before=%u after=%u",
                  rowClicked ? 1u : 0u,
                  craftClicked ? 1u : 0u,
                  static_cast<unsigned>(merchantFrame),
                  static_cast<unsigned>(itemRowFrame),
                  static_cast<unsigned>(rowClickFrame),
                  merchantItemPosition == UINT32_MAX ? 0xFFFFFFFFu : merchantItemPosition,
                  static_cast<unsigned>(pathActionFrame),
                  static_cast<unsigned>(actionPrimaryByContext),
                  static_cast<unsigned>(actionAltByContext),
                  static_cast<unsigned>(craftButtonFrame),
                  beforeCount,
                  afterCount);
    }
    return craftClicked;
}

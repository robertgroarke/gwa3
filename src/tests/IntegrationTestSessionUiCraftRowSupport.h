#include <gwa3/managers/MerchantMgr.h>
bool IsConsumableCraftRowOnlyMode(ConsumableHarnessClickMode clickMode) {
    return clickMode == ConsumableHarnessClickMode::RowOnly
        || clickMode == ConsumableHarnessClickMode::RowChild0Only
        || clickMode == ConsumableHarnessClickMode::RowChild1Only;
}

bool TryClickConsumableCraftRow(const ConsumableCraftUiFrames& frames,
                                const char* targetLabel,
                                uint32_t targetModelId,
                                uint32_t targetItemId,
                                ConsumableHarnessClickMode clickMode,
                                uint32_t beforeCount,
                                char* detail,
                                size_t detailSize) {
    if (frames.itemRowFrame < 0x10000) return false;

    if (detail && detailSize) {
        sprintf_s(detail, detailSize,
                  "row_click_start mode=%s row=0x%08X rowClick=0x%08X itemPos=%u actionPath=0x%08X action125=0x%08X action126=0x%08X before=%u",
                  DescribeConsumableHarnessClickMode(clickMode),
                  static_cast<unsigned>(frames.itemRowFrame),
                  static_cast<unsigned>(frames.rowClickFrame),
                  frames.merchantItemPosition == UINT32_MAX ? 0xFFFFFFFFu : frames.merchantItemPosition,
                  static_cast<unsigned>(frames.pathActionFrame),
                  static_cast<unsigned>(frames.actionPrimaryByContext),
                  static_cast<unsigned>(frames.actionAltByContext),
                  beforeCount);
        WriteConsumableHarnessStatus("row_click_start", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, beforeCount, 1, detail);
    }

    const bool rowClicked = UIMgr::ButtonClick(frames.rowClickFrame ? frames.rowClickFrame : frames.itemRowFrame);
    Sleep(500 + ChatMgr::GetPing());
    const uint32_t afterRowCount = CountInventoryModelQuantity(targetModelId);
    if (detail && detailSize) {
        sprintf_s(detail, detailSize,
                  "row_click_complete rowClicked=%u mode=%s row=0x%08X rowClick=0x%08X afterRow=%u",
                  rowClicked ? 1u : 0u,
                  DescribeConsumableHarnessClickMode(clickMode),
                  static_cast<unsigned>(frames.itemRowFrame),
                  static_cast<unsigned>(frames.rowClickFrame),
                  afterRowCount);
        WriteConsumableHarnessStatus("row_click_complete", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, afterRowCount, rowClicked ? 1u : 0u, detail);
    }
    return rowClicked;
}

bool TryRecoverConsumableCraftAfterRowClickFailure(const ConsumableCraftUiFrames& frames,
                                                   const char* targetLabel,
                                                   uint32_t targetModelId,
                                                   uint32_t targetItemId,
                                                   ConsumableHarnessClickMode clickMode,
                                                   uint32_t beforeCount,
                                                   uint32_t& afterCount,
                                                   char* detail,
                                                   size_t detailSize) {
    afterCount = CountInventoryModelQuantity(targetModelId);
    char rowFailDetail[256] = {};
    sprintf_s(rowFailDetail,
              "row_click_failed mode=%s row=0x%08X rowClick=0x%08X itemPos=%u merchantFrame=0x%08X actionPath=0x%08X action125=0x%08X action126=0x%08X",
              DescribeConsumableHarnessClickMode(clickMode),
              static_cast<unsigned>(frames.itemRowFrame),
              static_cast<unsigned>(frames.rowClickFrame),
              frames.merchantItemPosition == UINT32_MAX ? 0xFFFFFFFFu : frames.merchantItemPosition,
              static_cast<unsigned>(frames.merchantFrame),
              static_cast<unsigned>(frames.pathActionFrame),
              static_cast<unsigned>(frames.actionPrimaryByContext),
              static_cast<unsigned>(frames.actionAltByContext));
    WriteConsumableHarnessStatus("row_click_failed", targetLabel, ReadMapId(), 0,
                                 MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, afterCount, 0, rowFailDetail);

    const bool allowDirectWalkFallback =
        frames.merchantItemPosition != UINT32_MAX
        && frames.merchantFrame >= 0x10000
        && !IsConsumableCraftRowOnlyMode(clickMode);
    if (allowDirectWalkFallback) {
        WriteConsumableHarnessStatus("direct_walk_fallback_start", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, afterCount, 1, rowFailDetail);

        const bool nativeCrafted = CraftConsumableNatively(
            targetLabel, targetModelId, targetItemId, frames.merchantItemPosition, beforeCount, afterCount, detail, detailSize);
        if (nativeCrafted) return true;

        const uintptr_t candidates[] = { frames.actionPrimaryByContext, frames.actionAltByContext };
        const char* candidateLabels[] = { "action125", "action126" };
        for (uint32_t ci = 0; ci < 2; ++ci) {
            const uintptr_t craftBtn = candidates[ci];
            if (craftBtn < 0x10000 || UIMgr::IsFrameHidden(craftBtn)) continue;

            const uint32_t goldBefore2 = ItemMgr::GetGoldCharacter();
            MerchantStoCTap tap2{};
            StartMerchantStoCTap(tap2);
            CtoS::ResetPacketTap();

            const bool craftClicked2 = UIMgr::ButtonClick(craftBtn);
            IntReport("  Fallback %s click: frame=0x%08X hash=%u clicked=%u gold=%u",
                      candidateLabels[ci], static_cast<unsigned>(craftBtn),
                      UIMgr::GetFrameHash(craftBtn), craftClicked2 ? 1u : 0u, goldBefore2);

            if (craftClicked2) {
                WriteConsumableHarnessStatus("fallback_craft_clicked", targetLabel, ReadMapId(), 0,
                                             MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                             beforeCount, beforeCount, 1, candidateLabels[ci]);
                const bool craftObserved2 = WaitFor("fallback craft result", 3000, [targetModelId, beforeCount, goldBefore2]() {
                    return CountInventoryModelQuantity(targetModelId) > beforeCount
                        || ItemMgr::GetGoldCharacter() < goldBefore2;
                });
                afterCount = CountInventoryModelQuantity(targetModelId);
                const uint32_t goldAfter2 = ItemMgr::GetGoldCharacter();
                char stoCSummary2[256] = {};
                char ctoSSummary2[256] = {};
                FormatMerchantStoCTapSummary(stoCSummary2, sizeof(stoCSummary2), tap2);
                FormatCtoSPacketTapSummary(ctoSSummary2, sizeof(ctoSSummary2), CtoS::GetPacketTapSnapshot());
                StopMerchantStoCTap(tap2);
                if (detail && detailSize) {
                    sprintf_s(detail, detailSize,
                              "fallback_%s observed=%u before=%u after=%u gold=%u->%u stoC=%s ctoS=%s",
                              candidateLabels[ci], craftObserved2 ? 1u : 0u,
                              beforeCount, afterCount, goldBefore2, goldAfter2, stoCSummary2, ctoSSummary2);
                }
                WriteConsumableHarnessStatus("fallback_craft_result", targetLabel, ReadMapId(), 0,
                                             MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                             beforeCount, afterCount, afterCount > beforeCount ? 1u : 0u,
                                             detail ? detail : "");
                if (afterCount > beforeCount) {
                    IntReport("  FALLBACK %s CRAFT SUCCESS: before=%u after=%u gold=%u->%u",
                              candidateLabels[ci], beforeCount, afterCount, goldBefore2, goldAfter2);
                    return true;
                }
                IntReport("  Fallback %s: no delta (before=%u after=%u gold=%u->%u stoC=%s ctoS=%s)",
                          candidateLabels[ci], beforeCount, afterCount, goldBefore2, goldAfter2, stoCSummary2, ctoSSummary2);
            } else {
                StopMerchantStoCTap(tap2);
            }
        }
    }

    if (detail && detailSize) {
        sprintf_s(detail, detailSize,
                  "ui rowClicked=0 craftClicked=0 mode=%s row=0x%08X rowClick=0x%08X itemPos=%u before=%u after=%u",
                  DescribeConsumableHarnessClickMode(clickMode),
                  static_cast<unsigned>(frames.itemRowFrame),
                  static_cast<unsigned>(frames.rowClickFrame),
                  frames.merchantItemPosition == UINT32_MAX ? 0xFFFFFFFFu : frames.merchantItemPosition,
                  beforeCount,
                  afterCount);
    }
    return false;
}

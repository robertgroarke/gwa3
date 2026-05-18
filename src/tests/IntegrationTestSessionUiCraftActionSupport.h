#include <gwa3/managers/MerchantMgr.h>
bool TryClickConsumableCraftButton(const char* targetLabel,
                                   uint32_t targetModelId,
                                   uint32_t targetItemId,
                                   uintptr_t craftButtonFrame,
                                   uint32_t beforeCount,
                                   uint32_t& afterCount,
                                   char* detail,
                                   size_t detailSize) {
    const uint32_t goldBefore = ItemMgr::GetGoldCharacter();
    MerchantStoCTap tap{};
    StartMerchantStoCTap(tap);
    CtoS::ResetPacketTap();

    const uintptr_t craftTarget = craftButtonFrame;
    bool craftBtnClicked = false;
    if (craftTarget >= 0x10000) {
        const bool isHidden = UIMgr::IsFrameHidden(craftTarget);
        IntReport("  Craft button: frame=0x%08X hash=%u hidden=%u - clicking via GameThread",
                  static_cast<unsigned>(craftTarget), UIMgr::GetFrameHash(craftTarget), isHidden ? 1u : 0u);
        craftBtnClicked = UIMgr::ButtonClick(craftTarget);
        IntReport("  Craft button click result: clicked=%u", craftBtnClicked ? 1u : 0u);
    } else {
        IntReport("  Craft button not resolved: frame=0x%08X", static_cast<unsigned>(craftTarget));
    }

    if (!craftBtnClicked) {
        StopMerchantStoCTap(tap);
        return false;
    }

    WriteConsumableHarnessStatus("craft_button_clicked", targetLabel, ReadMapId(), 0,
                                 MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, beforeCount, 1, "craft_button_clicked");

    const bool craftObserved = WaitFor("craft button result", 3000, [targetModelId, beforeCount, goldBefore]() {
        return CountInventoryModelQuantity(targetModelId) > beforeCount
            || ItemMgr::GetGoldCharacter() < goldBefore;
    });
    afterCount = CountInventoryModelQuantity(targetModelId);
    const uint32_t goldAfter = ItemMgr::GetGoldCharacter();
    char stoCSummary[256] = {};
    char ctoSSummary[256] = {};
    FormatMerchantStoCTapSummary(stoCSummary, sizeof(stoCSummary), tap);
    FormatCtoSPacketTapSummary(ctoSSummary, sizeof(ctoSSummary), CtoS::GetPacketTapSnapshot());
    StopMerchantStoCTap(tap);

    if (detail && detailSize) {
        sprintf_s(detail, detailSize,
                  "craft_button_result observed=%u before=%u after=%u gold=%u->%u stoC=%s ctoS=%s",
                  craftObserved ? 1u : 0u, beforeCount, afterCount, goldBefore, goldAfter, stoCSummary, ctoSSummary);
    }
    WriteConsumableHarnessStatus("craft_button_result", targetLabel, ReadMapId(), 0,
                                 MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, afterCount, afterCount > beforeCount ? 1u : 0u,
                                 detail ? detail : "");

    if (afterCount > beforeCount) {
        IntReport("  CRAFT SUCCESS: before=%u after=%u gold=%u->%u", beforeCount, afterCount, goldBefore, goldAfter);
        return true;
    }
    IntReport("  Craft button clicked but no delta: before=%u after=%u gold=%u->%u stoC=%s ctoS=%s",
              beforeCount, afterCount, goldBefore, goldAfter, stoCSummary, ctoSSummary);
    return false;
}

bool ClickConsumableCraftAction(uintptr_t frame, const char* stageLabel,
                                const char* targetLabel, uint32_t targetModelId, uint32_t targetItemId,
                                ConsumableHarnessClickMode clickMode, uint32_t beforeCount,
                                const ConsumableMaterialCounter (&materialsBefore)[7],
                                char (&materialDetail)[512]) {
    if (frame < 0x10000 || UIMgr::IsFrameHidden(frame)) {
        char localDetail[192] = {};
        sprintf_s(localDetail, "action_click_skipped stage=%s frame=0x%08X hidden=%u hash=%u state=0x%X mode=%s",
                  stageLabel ? stageLabel : "",
                  static_cast<unsigned>(frame),
                  (frame >= 0x10000 && UIMgr::IsFrameHidden(frame)) ? 1u : 0u,
                  UIMgr::GetFrameHash(frame),
                  UIMgr::GetFrameState(frame),
                  DescribeConsumableHarnessClickMode(clickMode));
        WriteConsumableHarnessStatus("action_click_skipped", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, beforeCount, 0, localDetail);
        return false;
    }

    char localDetail[160] = {};
    sprintf_s(localDetail, "action_click_start stage=%s frame=0x%08X mode=%s", stageLabel,
              static_cast<unsigned>(frame), DescribeConsumableHarnessClickMode(clickMode));
    WriteConsumableHarnessStatus("action_click_start", targetLabel, ReadMapId(), 0,
                                 MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, beforeCount, 1, localDetail);

    const bool clicked = UIMgr::ButtonClick(frame);
    const int postClickDelaysMs[] = { 50, 150, 350 };
    int accumulatedDelay = 0;
    for (int delayMs : postClickDelaysMs) {
        const int sleepMs = delayMs - accumulatedDelay;
        if (sleepMs > 0) Sleep(sleepMs);
        accumulatedDelay = delayMs;

        const uintptr_t merchantFrameNow = UIMgr::GetFrameByHash(kMerchantRootHash);
        const uintptr_t quantityPromptFrame = UIMgr::GetVisibleFrameByChildOffsetAndChildCount(
            kTradeQuantityPromptChildOffsetId, 1u, 16u, merchantFrameNow);
        const uintptr_t altActionNow = UIMgr::GetFrameByContextAndChildOffset(
            merchantFrameNow >= 0x10000 ? UIMgr::GetFrameContext(merchantFrameNow) : 0u,
            126u,
            merchantFrameNow);
        char probeDetail[256] = {};
        sprintf_s(probeDetail,
                  "post_action_probe stage=%s t=%d clicked=%u merchant=0x%08X items=%u qtyPrompt=0x%08X qtyChildCount=%u altAction=0x%08X altHidden=%u",
                  stageLabel ? stageLabel : "",
                  delayMs,
                  clicked ? 1u : 0u,
                  static_cast<unsigned>(merchantFrameNow),
                  MerchantMgr::GetMerchantItemCount(),
                  static_cast<unsigned>(quantityPromptFrame),
                  UIMgr::GetChildFrameCount(quantityPromptFrame),
                  static_cast<unsigned>(altActionNow),
                  (altActionNow >= 0x10000 && UIMgr::IsFrameHidden(altActionNow)) ? 1u : 0u);
        WriteConsumableHarnessStatus("post_action_probe", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, CountInventoryModelQuantity(targetModelId),
                                     quantityPromptFrame >= 0x10000 ? 1u : 0u, probeDetail);

        if (quantityPromptFrame >= 0x10000) {
            char dumpLabel[64] = {};
            sprintf_s(dumpLabel, "consumable_post_%s_t%d_qty_prompt", stageLabel ? stageLabel : "action", delayMs);
            UIMgr::DebugDumpChildFrames(quantityPromptFrame, dumpLabel, 12);
        } else {
            char dumpLabel[64] = {};
            sprintf_s(dumpLabel, "consumable_post_%s_t%d_visible_child2", stageLabel ? stageLabel : "action", delayMs);
            UIMgr::DebugDumpVisibleFramesByChildOffset(kTradeQuantityPromptChildOffsetId, dumpLabel, 12);
        }
    }

    const bool quantityPromptOpen = TradeMgr::IsTradeQuantityPromptOpen();
    if (quantityPromptOpen) {
        const uint32_t promptFrameBeforeConfirm = TradeMgr::GetTradeQuantityPromptFrame();
        const uint32_t promptChildCountBeforeConfirm = TradeMgr::GetTradeQuantityPromptChildCount();
        char confirmDetail[256] = {};
        sprintf_s(confirmDetail,
                  "quantity_prompt_confirm_start stage=%s frame=0x%08X childCount=%u",
                  stageLabel ? stageLabel : "",
                  promptFrameBeforeConfirm,
                  promptChildCountBeforeConfirm);
        WriteConsumableHarnessStatus("quantity_prompt_confirm_start", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, CountInventoryModelQuantity(targetModelId), 1, confirmDetail);

        char promptDetail[768] = {};
        FormatPromptChildSnapshot(promptDetail, sizeof(promptDetail), promptFrameBeforeConfirm);
        WriteConsumableHarnessStatus("quantity_prompt_children_before_confirm", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, CountInventoryModelQuantity(targetModelId), 1, promptDetail);
        FormatPromptNestedChildSnapshot(promptDetail, sizeof(promptDetail), promptFrameBeforeConfirm, 4u);
        WriteConsumableHarnessStatus("quantity_prompt_child4_before_confirm", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, CountInventoryModelQuantity(targetModelId), 1, promptDetail);
        FormatPromptNestedChildSnapshot(promptDetail, sizeof(promptDetail), promptFrameBeforeConfirm, 2u);
        WriteConsumableHarnessStatus("quantity_prompt_child2_before_confirm", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, CountInventoryModelQuantity(targetModelId), 1, promptDetail);
        FormatPromptNestedGrandchildSnapshot(promptDetail, sizeof(promptDetail), promptFrameBeforeConfirm, 2u, 0u);
        WriteConsumableHarnessStatus("quantity_prompt_child2_0_before_confirm", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, CountInventoryModelQuantity(targetModelId), 1, promptDetail);
        FormatPromptNestedChildSnapshot(promptDetail, sizeof(promptDetail), promptFrameBeforeConfirm, 5u);
        WriteConsumableHarnessStatus("quantity_prompt_child5_before_confirm", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, CountInventoryModelQuantity(targetModelId), 1, promptDetail);

        bool confirmed = ConfirmCrafterQuantityPromptOneDirect(targetLabel, targetModelId, targetItemId, beforeCount);
        if (!confirmed) {
            confirmed = TradeMgr::ConfirmTradeQuantityPromptValue(1u);
        }
        sprintf_s(confirmDetail,
                  "quantity_prompt_confirm_value_complete stage=%s confirmed=%u quantity=1 frame=0x%08X childCount=%u inventory=%u",
                  stageLabel ? stageLabel : "",
                  confirmed ? 1u : 0u,
                  TradeMgr::GetTradeQuantityPromptFrame(),
                  TradeMgr::GetTradeQuantityPromptChildCount(),
                  CountInventoryModelQuantity(targetModelId));
        WriteConsumableHarnessStatus("quantity_prompt_confirm_value_complete", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, CountInventoryModelQuantity(targetModelId), confirmed ? 1u : 0u, confirmDetail);
        if (!confirmed) {
            confirmed = TradeMgr::ConfirmTradeQuantityPromptMax();
        }

        Sleep(250 + ChatMgr::GetPing());
        const uint32_t promptFrameAfterConfirm = TradeMgr::GetTradeQuantityPromptFrame();
        const uint32_t promptChildCountAfterConfirm = TradeMgr::GetTradeQuantityPromptChildCount();
        ConsumableMaterialCounter materialsAfterConfirm[7]{};
        FillConsumableMaterialCounters(materialsAfterConfirm);
        sprintf_s(confirmDetail,
                  "quantity_prompt_confirm_complete stage=%s confirmed=%u frameAfter=0x%08X childCountAfter=%u inventory=%u",
                  stageLabel ? stageLabel : "",
                  confirmed ? 1u : 0u,
                  promptFrameAfterConfirm,
                  promptChildCountAfterConfirm,
                  CountInventoryModelQuantity(targetModelId));
        WriteConsumableHarnessStatus("quantity_prompt_confirm_complete", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, CountInventoryModelQuantity(targetModelId), confirmed ? 1u : 0u, confirmDetail);
        FormatPromptChildSnapshot(promptDetail, sizeof(promptDetail), promptFrameAfterConfirm);
        WriteConsumableHarnessStatus("quantity_prompt_children_after_confirm", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, CountInventoryModelQuantity(targetModelId), confirmed ? 1u : 0u, promptDetail);
        FormatPromptNestedChildSnapshot(promptDetail, sizeof(promptDetail), promptFrameAfterConfirm, 4u);
        WriteConsumableHarnessStatus("quantity_prompt_child4_after_confirm", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, CountInventoryModelQuantity(targetModelId), confirmed ? 1u : 0u, promptDetail);
        FormatPromptNestedChildSnapshot(promptDetail, sizeof(promptDetail), promptFrameAfterConfirm, 2u);
        WriteConsumableHarnessStatus("quantity_prompt_child2_after_confirm", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, CountInventoryModelQuantity(targetModelId), confirmed ? 1u : 0u, promptDetail);
        FormatPromptNestedGrandchildSnapshot(promptDetail, sizeof(promptDetail), promptFrameAfterConfirm, 2u, 0u);
        WriteConsumableHarnessStatus("quantity_prompt_child2_0_after_confirm", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, CountInventoryModelQuantity(targetModelId), confirmed ? 1u : 0u, promptDetail);
        FormatPromptNestedChildSnapshot(promptDetail, sizeof(promptDetail), promptFrameAfterConfirm, 5u);
        WriteConsumableHarnessStatus("quantity_prompt_child5_after_confirm", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, CountInventoryModelQuantity(targetModelId), confirmed ? 1u : 0u, promptDetail);
        FormatConsumableMaterialSnapshot(materialDetail, sizeof(materialDetail), materialsBefore, materialsAfterConfirm);
        WriteConsumableHarnessStatus("material_snapshot_after_confirm", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, CountInventoryModelQuantity(targetModelId), confirmed ? 1u : 0u, materialDetail);
    }

    Sleep((900 + ChatMgr::GetPing()) - accumulatedDelay);
    const uint32_t observedCount = CountInventoryModelQuantity(targetModelId);
    ConsumableMaterialCounter materialsAfterAction[7]{};
    FillConsumableMaterialCounters(materialsAfterAction);
    FormatConsumableMaterialSnapshot(materialDetail, sizeof(materialDetail), materialsBefore, materialsAfterAction);
    WriteConsumableHarnessStatus("material_snapshot_after_action", targetLabel, ReadMapId(), 0,
                                 MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, observedCount, clicked ? 1u : 0u, materialDetail);
    sprintf_s(localDetail, "action_click_complete stage=%s clicked=%u frame=0x%08X mode=%s after=%u",
              stageLabel, clicked ? 1u : 0u, static_cast<unsigned>(frame),
              DescribeConsumableHarnessClickMode(clickMode), observedCount);
    WriteConsumableHarnessStatus("action_click_complete", targetLabel, ReadMapId(), 0,
                                 MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 beforeCount, observedCount, clicked ? 1u : 0u, localDetail);
    return clicked;
}

bool TryConsumableCraftActionFallbacks(const ConsumableCraftUiFrames& frames,
                                       const char* targetLabel,
                                       uint32_t targetModelId,
                                       uint32_t targetItemId,
                                       ConsumableHarnessClickMode clickMode,
                                       uint32_t beforeCount,
                                       const ConsumableMaterialCounter (&materialsBefore)[7],
                                       char (&materialDetail)[512],
                                       char* detail,
                                       size_t detailSize) {
    if (detail && detailSize) {
        sprintf_s(detail, detailSize,
                  "native_packet_fallback mode=%s actionPath=0x%08X pathHidden=%u action125=0x%08X hidden125=%u action126=0x%08X hidden126=%u",
                  DescribeConsumableHarnessClickMode(clickMode),
                  static_cast<unsigned>(frames.pathActionFrame),
                  (frames.pathActionFrame >= 0x10000 && UIMgr::IsFrameHidden(frames.pathActionFrame)) ? 1u : 0u,
                  static_cast<unsigned>(frames.actionPrimaryByContext),
                  (frames.actionPrimaryByContext >= 0x10000 && UIMgr::IsFrameHidden(frames.actionPrimaryByContext)) ? 1u : 0u,
                  static_cast<unsigned>(frames.actionAltByContext),
                  (frames.actionAltByContext >= 0x10000 && UIMgr::IsFrameHidden(frames.actionAltByContext)) ? 1u : 0u);
        WriteConsumableHarnessStatus("native_packet_fallback", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, beforeCount, 1, detail);
    }

    bool craftClicked = false;
    if (clickMode == ConsumableHarnessClickMode::RootOnly || clickMode == ConsumableHarnessClickMode::Both) {
        craftClicked = ClickConsumableCraftAction(frames.actionPrimaryByContext, "action125",
                                                  targetLabel, targetModelId, targetItemId,
                                                  clickMode, beforeCount, materialsBefore, materialDetail)
            || craftClicked;
    }
    if (clickMode == ConsumableHarnessClickMode::PathOnly) {
        craftClicked = ClickConsumableCraftAction(frames.pathActionFrame, "actionPath",
                                                  targetLabel, targetModelId, targetItemId,
                                                  clickMode, beforeCount, materialsBefore, materialDetail)
            || craftClicked;
    }
    if (clickMode == ConsumableHarnessClickMode::AltOnly || clickMode == ConsumableHarnessClickMode::Both) {
        craftClicked = ClickConsumableCraftAction(frames.actionAltByContext ? frames.actionAltByContext : frames.craftButtonFrame, "action126",
                                                  targetLabel, targetModelId, targetItemId,
                                                  clickMode, beforeCount, materialsBefore, materialDetail)
            || craftClicked;
    }
    return craftClicked;
}

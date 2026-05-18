#include <gwa3/managers/MerchantMgr.h>
bool ConfirmCrafterQuantityPromptOneDirect(const char* targetLabel, uint32_t targetModelId, uint32_t targetItemId,
                                           uint32_t beforeCount) {
    const uintptr_t promptFrame = TradeMgr::GetTradeQuantityPromptFrame();
    if (promptFrame < 0x10000) return false;

    const uintptr_t promptButtonBar = UIMgr::GetChildFrameByIndex(promptFrame, 5u);
    const uintptr_t promptChild2 = UIMgr::GetChildFrameByIndex(promptFrame, 2u);
    const uintptr_t candidates[] = {
        UIMgr::GetChildFrameByIndex(promptChild2, 0u),
        UIMgr::GetChildFrameByIndex(promptButtonBar, 2u),
        UIMgr::GetChildFrameByIndex(promptButtonBar, 1u),
        UIMgr::GetChildFrameByIndex(promptFrame, 3u),
    };
    const char* labels[] = {
        "child2[0]",
        "bar[5][2]",
        "bar[5][1]",
        "root[3]",
    };

    for (size_t i = 0; i < _countof(candidates); ++i) {
        const uintptr_t candidate = candidates[i];
        if (candidate < 0x10000 || UIMgr::IsFrameHidden(candidate)) continue;

        char detail[192] = {};
        sprintf_s(detail, "direct_prompt_click_start candidate=%s frame=0x%08X", labels[i], static_cast<unsigned>(candidate));
        WriteConsumableHarnessStatus("quantity_prompt_direct_click_start", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, CountInventoryModelQuantity(targetModelId), 1, detail);

        const bool clicked = UIMgr::ButtonClick(candidate);
        Sleep(250 + ChatMgr::GetPing());
        const bool closed = !TradeMgr::IsTradeQuantityPromptOpen();
        sprintf_s(detail, "direct_prompt_click_complete candidate=%s frame=0x%08X clicked=%u closed=%u",
                  labels[i], static_cast<unsigned>(candidate), clicked ? 1u : 0u, closed ? 1u : 0u);
        WriteConsumableHarnessStatus("quantity_prompt_direct_click_complete", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     beforeCount, CountInventoryModelQuantity(targetModelId), closed ? 1u : 0u, detail);
        if (clicked && closed) return true;
    }

    if (promptChild2 >= 0x10000) {
        const uintptr_t child20 = UIMgr::GetChildFrameByIndex(promptChild2, 0u);
        const uintptr_t child200 = UIMgr::GetChildFrameByIndex(child20, 0u);
        const uintptr_t commitButtons[] = {
            UIMgr::GetChildFrameByIndex(promptButtonBar, 2u),
            UIMgr::GetChildFrameByIndex(promptButtonBar, 1u),
        };
        const char* commitLabels[] = { "bar[5][2]", "bar[5][1]" };
        const wchar_t quantityBuf[] = L"1";

        const auto trySetAndCommit = [&](const char* mode, bool setOk) -> bool {
            char detail[224] = {};
            sprintf_s(detail, "direct_prompt_set_%s child2[0]=0x%08X parent=0x%08X setOk=%u",
                      mode, static_cast<unsigned>(child20), static_cast<unsigned>(promptChild2), setOk ? 1u : 0u);
            WriteConsumableHarnessStatus("quantity_prompt_set_start", targetLabel, ReadMapId(), 0,
                                         MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                         beforeCount, CountInventoryModelQuantity(targetModelId), setOk ? 1u : 0u, detail);
            if (!setOk) return false;

            for (size_t i = 0; i < _countof(commitButtons); ++i) {
                const uintptr_t commit = commitButtons[i];
                if (commit < 0x10000 || UIMgr::IsFrameHidden(commit)) continue;
                const bool clicked = UIMgr::ButtonClick(commit);
                Sleep(250 + ChatMgr::GetPing());
                const bool closed = !TradeMgr::IsTradeQuantityPromptOpen();
                sprintf_s(detail, "direct_prompt_set_%s_commit candidate=%s frame=0x%08X clicked=%u closed=%u",
                          mode, commitLabels[i], static_cast<unsigned>(commit), clicked ? 1u : 0u, closed ? 1u : 0u);
                WriteConsumableHarnessStatus("quantity_prompt_set_commit_complete", targetLabel, ReadMapId(), 0,
                                             MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                             beforeCount, CountInventoryModelQuantity(targetModelId), closed ? 1u : 0u, detail);
                if (clicked && closed) return true;
            }
            return false;
        };

        if (child20 >= 0x10000 && trySetAndCommit("editable", UIMgr::SetEditableTextValue(child20, quantityBuf, promptChild2))) {
            return true;
        }
        if (child20 >= 0x10000 && trySetAndCommit("numeric", UIMgr::SetNumericFrameValue(child20, 1u, promptChild2))) {
            return true;
        }
        if (child200 >= 0x10000 && trySetAndCommit("editable_nested", UIMgr::SetEditableTextValue(child200, quantityBuf, child20))) {
            return true;
        }
        if (child200 >= 0x10000 && trySetAndCommit("numeric_nested", UIMgr::SetNumericFrameValue(child200, 1u, child20))) {
            return true;
        }
    }

    return false;
}

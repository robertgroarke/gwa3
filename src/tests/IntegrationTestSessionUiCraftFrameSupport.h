#include <gwa3/managers/MerchantMgr.h>
uint32_t FindMerchantItemPositionByModelId(uint32_t modelId) {
    const uint32_t merchantCount = MerchantMgr::GetMerchantItemCount();
    for (uint32_t i = 0; i < merchantCount; ++i) {
        Item* item = MerchantMgr::GetMerchantItemByPosition(i);
        if (item && item->model_id == modelId) return i;
    }
    return UINT32_MAX;
}

uintptr_t ResolveMerchantSortedPathFrame(uintptr_t merchantFrame, const uint32_t* path, uint32_t pathLen, const char* label) {
    const uintptr_t frame = UIMgr::NavigateSortedChildPath(merchantFrame, path, pathLen);
    IntReport("  Merchant path %s: frame=0x%08X hash=%u childOffset=%u childCount=%u context=0x%08X",
              label ? label : "",
              static_cast<unsigned>(frame),
              UIMgr::GetFrameHash(frame),
              UIMgr::GetChildOffsetId(frame),
              UIMgr::GetChildFrameCount(frame),
              static_cast<unsigned>(UIMgr::GetFrameContext(frame)));
    return frame;
}

uintptr_t ResolveMerchantRowClickTarget(uintptr_t itemRowFrame, ConsumableHarnessClickMode clickMode) {
    if (itemRowFrame < 0x10000) return 0;
    if (clickMode == ConsumableHarnessClickMode::RowChild0Only) {
        return UIMgr::GetChildFrameByIndex(itemRowFrame, 0u);
    }
    if (clickMode == ConsumableHarnessClickMode::RowChild1Only) {
        return UIMgr::GetChildFrameByIndex(itemRowFrame, 1u);
    }
    return itemRowFrame;
}

struct ConsumableCraftUiFrames {
    uintptr_t merchantFrame = 0;
    uintptr_t merchantContext = 0;
    uint32_t merchantItemPosition = UINT32_MAX;
    uintptr_t itemRowFrame = 0;
    uintptr_t rowClickFrame = 0;
    uintptr_t pathActionFrame = 0;
    uintptr_t actionPrimaryByContext = 0;
    uintptr_t actionAltByContext = 0;
    uintptr_t craftButtonFrame = 0;
};

ConsumableCraftUiFrames ResolveConsumableCraftUiFrames(const char* targetLabel,
                                                       uint32_t targetModelId,
                                                       uint32_t targetItemId,
                                                       ConsumableHarnessClickMode clickMode) {
    WriteConsumableHarnessStatus("ui_probe_start", targetLabel, ReadMapId(), 0,
                                 MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                 0, 0, 1, "resolving_merchant_frames");

    ConsumableCraftUiFrames frames{};
    frames.merchantFrame = UIMgr::GetFrameByHash(kMerchantRootHash);
    {
        char probeDetail[192] = {};
        sprintf_s(probeDetail, "merchantFrame=0x%08X", static_cast<unsigned>(frames.merchantFrame));
        WriteConsumableHarnessStatus("ui_probe_merchant_frame", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     0, 0, frames.merchantFrame >= 0x10000 ? 1u : 0u, probeDetail);
    }

    frames.merchantContext = UIMgr::GetFrameContext(frames.merchantFrame);
    frames.merchantItemPosition = FindMerchantItemPositionByModelId(targetModelId);
    {
        char probeDetail[192] = {};
        sprintf_s(probeDetail, "merchantContext=0x%08X itemPos=%u",
                  static_cast<unsigned>(frames.merchantContext),
                  frames.merchantItemPosition == UINT32_MAX ? 0xFFFFFFFFu : frames.merchantItemPosition);
        WriteConsumableHarnessStatus("ui_probe_item_position", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     0, 0, frames.merchantItemPosition != UINT32_MAX ? 1u : 0u, probeDetail);
    }

    // AutoIt's proven working item selection: NavigateFramePath("0,0,itemIndex")
    // itemIndex is 0-based. merchantItemPosition is 1-based, so subtract 1.
    const uint32_t autoit_itemIndex =
        (frames.merchantItemPosition == UINT32_MAX || frames.merchantItemPosition == 0u)
            ? 0u
            : (frames.merchantItemPosition - 1u);
    const uint32_t autoitItemPath[] = { 0u, 0u, autoit_itemIndex };
    const uint32_t actionButtonPath[] = { 0u, 1u, 1u };
    frames.itemRowFrame = frames.merchantItemPosition == UINT32_MAX
        ? 0u
        : ResolveMerchantSortedPathFrame(frames.merchantFrame, autoitItemPath, _countof(autoitItemPath), "item[0,0,index] (AutoIt)");
    if (frames.itemRowFrame < 0x10000 && frames.merchantItemPosition != UINT32_MAX) {
        const uint32_t itemRowPath[] = { 0u, 1u, 3u, 0u, autoit_itemIndex };
        frames.itemRowFrame = ResolveMerchantSortedPathFrame(frames.merchantFrame, itemRowPath, _countof(itemRowPath), "item[0,1,3,0,index-1]-fallback");
    }

    frames.rowClickFrame = ResolveMerchantRowClickTarget(frames.itemRowFrame, clickMode);
    frames.pathActionFrame = ResolveMerchantSortedPathFrame(frames.merchantFrame, actionButtonPath, _countof(actionButtonPath), "action[0,1,1]");
    frames.actionPrimaryByContext = frames.merchantContext >= 0x10000
        ? UIMgr::GetFrameByContextAndChildOffset(frames.merchantContext, 125u, frames.merchantFrame)
        : 0u;
    frames.actionAltByContext = frames.merchantContext >= 0x10000
        ? UIMgr::GetFrameByContextAndChildOffset(frames.merchantContext, 126u, frames.merchantFrame)
        : 0u;
    // action125 (childOffset 125 in merchant context) produces CtoS 0x049 when clicked.
    // pathActionFrame {0,1,1} has wrong sub-context and crashes on immediate click.
    // Use action125 via GameThread dispatch (ButtonClick, not ButtonClickImmediate).
    frames.craftButtonFrame = frames.actionPrimaryByContext
        ? frames.actionPrimaryByContext
        : (frames.pathActionFrame
            ? frames.pathActionFrame
            : (frames.actionAltByContext ? frames.actionAltByContext : UIMgr::GetFrameByHash(kMerchantActionButtonAltHash)));

    {
        char probeDetail[256] = {};
        sprintf_s(probeDetail,
                  "row=0x%08X rowClick=0x%08X actionPath=0x%08X action125=0x%08X action126=0x%08X craftButton=0x%08X",
                  static_cast<unsigned>(frames.itemRowFrame),
                  static_cast<unsigned>(frames.rowClickFrame),
                  static_cast<unsigned>(frames.pathActionFrame),
                  static_cast<unsigned>(frames.actionPrimaryByContext),
                  static_cast<unsigned>(frames.actionAltByContext),
                  static_cast<unsigned>(frames.craftButtonFrame));
        WriteConsumableHarnessStatus("ui_probe_frames_resolved", targetLabel, ReadMapId(), 0,
                                     MerchantMgr::GetMerchantItemCount(), targetModelId, targetItemId,
                                     0, 0, 1, probeDetail);
    }

    const uintptr_t rowContext = UIMgr::GetFrameContext(frames.itemRowFrame);
    const uintptr_t rowClickContext = UIMgr::GetFrameContext(frames.rowClickFrame);
    IntReport("  UI craft probe for %s: clickMode=%s merchantFrame=0x%08X context=0x%08X row=0x%08X rowHash=%u rowChildOffset=%u rowContext=0x%08X rowClick=0x%08X rowClickHash=%u rowClickChildOffset=%u rowClickContext=0x%08X actionPath=0x%08X action125=0x%08X action126=0x%08X craftButton=0x%08X targetModel=%u item=%u itemPos=%u",
              targetLabel ? targetLabel : "",
              DescribeConsumableHarnessClickMode(clickMode),
              static_cast<unsigned>(frames.merchantFrame),
              static_cast<unsigned>(frames.merchantContext),
              static_cast<unsigned>(frames.itemRowFrame),
              UIMgr::GetFrameHash(frames.itemRowFrame),
              UIMgr::GetChildOffsetId(frames.itemRowFrame),
              static_cast<unsigned>(rowContext),
              static_cast<unsigned>(frames.rowClickFrame),
              UIMgr::GetFrameHash(frames.rowClickFrame),
              UIMgr::GetChildOffsetId(frames.rowClickFrame),
              static_cast<unsigned>(rowClickContext),
              static_cast<unsigned>(frames.pathActionFrame),
              static_cast<unsigned>(frames.actionPrimaryByContext),
              static_cast<unsigned>(frames.actionAltByContext),
              static_cast<unsigned>(frames.craftButtonFrame),
              targetModelId,
              targetItemId,
              frames.merchantItemPosition == UINT32_MAX ? 0xFFFFFFFFu : frames.merchantItemPosition);

    return frames;
}

void AppendConsumableFrameDumpLine(const char* line) {
    if (!line) return;
    char path[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&AppendConsumableFrameDumpLine), &hSelf);
    GetModuleFileNameA(hSelf, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, "consumable_harness_frame_dump.txt");
    HANDLE h = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(h, line, static_cast<DWORD>(strlen(line)), &written, nullptr);
    CloseHandle(h);
}

void AppendConsumableFrameDumpMarker(const char* label) {
    char line[256];
    const DWORD pid = GetCurrentProcessId();
    static const DWORD runTag = GetTickCount();
    sprintf_s(line, "pid=%lu run_tag=%lu marker=%s\r\n", pid, runTag, label ? label : "(null)");
    AppendConsumableFrameDumpLine(line);
}

void DumpConsumableFrameSummary(const char* label, uintptr_t frame) {
    char line[512];
    if (frame < 0x10000) {
        sprintf_s(line, "  %s frame=0x00000000\r\n", label ? label : "(null)");
        AppendConsumableFrameDumpLine(line);
        return;
    }
    const uint32_t childCount = UIMgr::GetChildFrameCount(frame);
    sprintf_s(line,
              "  %s frame=0x%08X hash=%u state=0x%X frameId=%u childOffset=%u childCount=%u context=0x%08X\r\n",
              label ? label : "(null)",
              static_cast<unsigned>(frame),
              UIMgr::GetFrameHash(frame),
              UIMgr::GetFrameState(frame),
              UIMgr::GetFrameId(frame),
              UIMgr::GetChildOffsetId(frame),
              childCount,
              static_cast<unsigned>(UIMgr::GetFrameContext(frame)));
    AppendConsumableFrameDumpLine(line);
}

void DumpConsumableFrameTree(uintptr_t merchantFrame, uint32_t merchantItemPosition, uint32_t targetModelId, uint32_t targetItemId) {
    const DWORD pid = GetCurrentProcessId();
    static const DWORD runTag = GetTickCount();
    char line[512];
    sprintf_s(line, "pid=%lu run_tag=%lu merchant=0x%08X target_model=%u target_item=%u item_pos=%u\r\n",
              pid, runTag, static_cast<unsigned>(merchantFrame), targetModelId, targetItemId, merchantItemPosition);
    AppendConsumableFrameDumpLine(line);

    const uintptr_t merchantContext = UIMgr::GetFrameContext(merchantFrame);
    sprintf_s(line, "  merchant_context=0x%08X\r\n", static_cast<unsigned>(merchantContext));
    AppendConsumableFrameDumpLine(line);

    AppendConsumableFrameDumpMarker("dump_root_0_begin");
    const uintptr_t root0 = UIMgr::GetChildFrameByIndex(merchantFrame, 0u);
    DumpConsumableFrameSummary("root[0]", root0);

    AppendConsumableFrameDumpMarker("dump_root_0_1_begin");
    const uintptr_t root01 = UIMgr::GetChildFrameByIndex(root0, 1u);
    DumpConsumableFrameSummary("root[0][1]", root01);

    const uint32_t branchIndices[] = { 2u, 3u };
    for (uint32_t branchIndex = 0; branchIndex < _countof(branchIndices); ++branchIndex) {
        const uint32_t childIndex = branchIndices[branchIndex];
        sprintf_s(line, "dump_root_0_1_%u_begin", childIndex);
        AppendConsumableFrameDumpMarker(line);

        const uintptr_t branch = UIMgr::GetChildFrameByIndex(root01, childIndex);
        char label[64];
        sprintf_s(label, "root[0][1][%u]", childIndex);
        DumpConsumableFrameSummary(label, branch);
        if (branch < 0x10000) continue;

        const uint32_t branchChildCount = UIMgr::GetChildFrameCount(branch);
        const uint32_t branchChildCapped = (branchChildCount < 8u) ? branchChildCount : 8u;
        for (uint32_t child = 0; child < branchChildCapped; ++child) {
            const uintptr_t row = UIMgr::GetChildFrameByIndex(branch, child);
            sprintf_s(label, "root[0][1][%u][%u]", childIndex, child);
            DumpConsumableFrameSummary(label, row);

            if (row < 0x10000) continue;
            const uint32_t rowChildCount = UIMgr::GetChildFrameCount(row);
            const uint32_t rowChildCapped = (rowChildCount < 4u) ? rowChildCount : 4u;
            for (uint32_t leaf = 0; leaf < rowChildCapped; ++leaf) {
                const uintptr_t leafFrame = UIMgr::GetChildFrameByIndex(row, leaf);
                sprintf_s(label, "root[0][1][%u][%u][%u]", childIndex, child, leaf);
                DumpConsumableFrameSummary(label, leafFrame);
            }

            if (childIndex == 3u && child == 0u) {
                AppendConsumableFrameDumpMarker("dump_root_0_1_3_0_full_begin");
                const uint32_t fullChildCapped = (rowChildCount < 8u) ? rowChildCount : 8u;
                for (uint32_t fullLeaf = rowChildCapped; fullLeaf < fullChildCapped; ++fullLeaf) {
                    const uintptr_t leafFrame = UIMgr::GetChildFrameByIndex(row, fullLeaf);
                    sprintf_s(label, "root[0][1][3][0][%u]", fullLeaf);
                    DumpConsumableFrameSummary(label, leafFrame);
                }
            }
        }
    }
}

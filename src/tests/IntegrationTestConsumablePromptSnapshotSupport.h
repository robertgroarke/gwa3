void FormatPromptChildSnapshot(char* out, size_t outSize, uintptr_t promptFrame, uint32_t maxChildren = 6u) {
    if (!out || !outSize) return;
    if (promptFrame < 0x10000) {
        sprintf_s(out, outSize, "prompt=0x%08X childCount=0", static_cast<unsigned>(promptFrame));
        return;
    }

    const uint32_t childCount = UIMgr::GetChildFrameCount(promptFrame);
    const uint32_t emitCount = childCount < maxChildren ? childCount : maxChildren;
    int written = sprintf_s(out, outSize, "prompt=0x%08X childCount=%u",
                            static_cast<unsigned>(promptFrame), childCount);
    if (written < 0) return;

    for (uint32_t i = 0; i < emitCount && static_cast<size_t>(written) < outSize; ++i) {
        const uintptr_t child = UIMgr::GetChildFrameByIndex(promptFrame, i);
        const int appended = sprintf_s(
            out + written, outSize - written,
            " i%u=0x%08X/h%u/o%u/s%X/c%u",
            i,
            static_cast<unsigned>(child),
            UIMgr::GetFrameHash(child),
            UIMgr::GetChildOffsetId(child),
            UIMgr::GetFrameState(child),
            UIMgr::GetChildFrameCount(child));
        if (appended < 0) break;
        written += appended;
    }
}

void FormatPromptNestedChildSnapshot(char* out, size_t outSize, uintptr_t promptFrame,
                                     uint32_t parentChildIndex, uint32_t maxChildren = 6u) {
    if (!out || !outSize) return;
    const uintptr_t parent = UIMgr::GetChildFrameByIndex(promptFrame, parentChildIndex);
    if (parent < 0x10000) {
        sprintf_s(out, outSize, "prompt=0x%08X parentIndex=%u parent=0x%08X",
                  static_cast<unsigned>(promptFrame),
                  parentChildIndex,
                  static_cast<unsigned>(parent));
        return;
    }

    const uint32_t childCount = UIMgr::GetChildFrameCount(parent);
    const uint32_t emitCount = childCount < maxChildren ? childCount : maxChildren;
    int written = sprintf_s(out, outSize,
                            "prompt=0x%08X parentIndex=%u parent=0x%08X hash=%u childCount=%u",
                            static_cast<unsigned>(promptFrame),
                            parentChildIndex,
                            static_cast<unsigned>(parent),
                            UIMgr::GetFrameHash(parent),
                            childCount);
    if (written < 0) return;

    for (uint32_t i = 0; i < emitCount && static_cast<size_t>(written) < outSize; ++i) {
        const uintptr_t child = UIMgr::GetChildFrameByIndex(parent, i);
        const int appended = sprintf_s(
            out + written, outSize - written,
            " i%u=0x%08X/h%u/o%u/s%X/c%u",
            i,
            static_cast<unsigned>(child),
            UIMgr::GetFrameHash(child),
            UIMgr::GetChildOffsetId(child),
            UIMgr::GetFrameState(child),
            UIMgr::GetChildFrameCount(child));
        if (appended < 0) break;
        written += appended;
    }
}

void FormatPromptNestedGrandchildSnapshot(char* out, size_t outSize, uintptr_t promptFrame,
                                          uint32_t parentChildIndex, uint32_t childIndex,
                                          uint32_t maxChildren = 6u) {
    if (!out || !outSize) return;
    const uintptr_t parent = UIMgr::GetChildFrameByIndex(promptFrame, parentChildIndex);
    const uintptr_t child = UIMgr::GetChildFrameByIndex(parent, childIndex);
    if (child < 0x10000) {
        sprintf_s(out, outSize, "prompt=0x%08X parentIndex=%u childIndex=%u child=0x%08X",
                  static_cast<unsigned>(promptFrame), parentChildIndex, childIndex, static_cast<unsigned>(child));
        return;
    }
    size_t used = 0;
    const uint32_t childCount = UIMgr::GetChildFrameCount(child);
    used += sprintf_s(out + used, outSize - used,
                      "prompt=0x%08X parentIndex=%u childIndex=%u node=0x%08X hash=%u childCount=%u",
                      static_cast<unsigned>(promptFrame),
                      parentChildIndex,
                      childIndex,
                      static_cast<unsigned>(child),
                      UIMgr::GetFrameHash(child),
                      childCount);
    const uint32_t capped = childCount < maxChildren ? childCount : maxChildren;
    for (uint32_t i = 0; i < capped && used < outSize; ++i) {
        const uintptr_t nested = UIMgr::GetChildFrameByIndex(child, i);
        used += sprintf_s(out + used, outSize - used,
                          " i%u=0x%08X/h%u/o%u/s%X/c%u",
                          i,
                          static_cast<unsigned>(nested),
                          UIMgr::GetFrameHash(nested),
                          UIMgr::GetChildOffsetId(nested),
                          UIMgr::GetFrameState(nested),
                          UIMgr::GetChildFrameCount(nested));
    }
}

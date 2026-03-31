#pragma once

#include <cstdint>

namespace GWA3::UIMgr {

    // Frame state flags
    constexpr uint32_t FRAME_CREATED  = 0x4;
    constexpr uint32_t FRAME_DISABLED = 0x10;
    constexpr uint32_t FRAME_HIDDEN   = 0x200;

    // Frame message IDs
    constexpr uint32_t MSG_MOUSE_CLICK2 = 0x31;

    // MouseAction action states
    constexpr uint32_t ACTION_MOUSE_UP   = 0x7;
    constexpr uint32_t ACTION_MOUSE_DOWN = 0x6;

    // Known frame hashes (character select and common UI)
    namespace Hashes {
        constexpr uint32_t PlayButton          = 184818986;
        constexpr uint32_t PlayButtonGreyed     = 41327607;
        constexpr uint32_t ReconnectYes         = 1398610279;
        constexpr uint32_t ReconnectNo          = 3600335809;
        constexpr uint32_t CreateButton         = 3372446797;
        constexpr uint32_t DeleteButton         = 3379687503;
        constexpr uint32_t LogOutButton         = 1117342925;
        constexpr uint32_t CharacterFrame       = 828467986;
        constexpr uint32_t EditAccount          = 1601494406;
    }

    bool Initialize();

    // Frame lookup
    uintptr_t GetFrameByHash(uint32_t hash);
    uintptr_t GetRootFrame();

    // Frame state queries
    uint32_t GetFrameId(uintptr_t frame);
    uint32_t GetChildOffsetId(uintptr_t frame);
    uint32_t GetFrameState(uintptr_t frame);
    uint32_t GetFrameHash(uintptr_t frame);
    bool IsFrameCreated(uintptr_t frame);
    bool IsFrameHidden(uintptr_t frame);
    bool IsFrameDisabled(uintptr_t frame);
    bool IsFrameVisible(uint32_t hash);

    // Frame context (parent frame for SendFrameUIMsg)
    uintptr_t GetFrameContext(uintptr_t frame);

    // Send frame UI message (must be on game thread)
    void SendFrameUIMessage(uintptr_t frame, uint32_t msgId, void* wParam, void* lParam);
    void SendUIMessage(uint32_t msgId, void* wParam, void* lParam);

    // Button click (GWA3-021)
    // Sends MouseUp(0x7) via SendFrameUIMsg with msgid=0x31
    bool ButtonClick(uintptr_t frame);
    bool ButtonClickByHash(uint32_t hash);

} // namespace GWA3::UIMgr

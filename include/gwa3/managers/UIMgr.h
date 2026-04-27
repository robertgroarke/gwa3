#pragma once

#include <cstdint>

namespace GWA3::UIMgr {

    enum class ControlAction : uint32_t {
        UseSkill1 = 0xA4,
        UseSkill2 = 0xA5,
        UseSkill3 = 0xA6,
        UseSkill4 = 0xA7,
        UseSkill5 = 0xA8,
        UseSkill6 = 0xA9,
        UseSkill7 = 0xAA,
        UseSkill8 = 0xAB,
    };

    // Frame state flags
    constexpr uint32_t FRAME_CREATED  = 0x4;
    constexpr uint32_t FRAME_DISABLED = 0x10;
    constexpr uint32_t FRAME_HIDDEN   = 0x200;

    // Frame message IDs
    constexpr uint32_t MSG_MOUSE_CLICK2 = 0x31;
    constexpr uint32_t MSG_INITIATE_TRADE = 0x100001A0;

    // MouseAction action states
    constexpr uint32_t ACTION_MOUSE_UP   = 0x7;
    constexpr uint32_t ACTION_MOUSE_DOWN = 0x6;
    constexpr uint32_t ACTION_MOUSE_CLICK = 0x8;

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
        constexpr uint32_t ReturnToOutpost     = 22386219;
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

    // Send UI message (must be on game thread)
    void SendUIMessage(uint32_t msgId, void* wParam, void* lParam);

    // Child frame navigation
    uint32_t GetChildFrameCount(uintptr_t frame);
    uintptr_t GetChildFrameByIndex(uintptr_t frame, uint32_t index);
    uintptr_t GetChildFrameByOffset(uintptr_t frame, uint32_t childOffsetId);

    uintptr_t GetFrameById(uint32_t frameId);
    uintptr_t GetFrameByContextAndChildOffset(uintptr_t context, uint32_t childOffsetId, uintptr_t excludeFrame = 0);
    uintptr_t GetVisibleFrameByChildOffset(uint32_t childOffsetId, uintptr_t excludeFrame = 0, uintptr_t excludeContext = 0);

    // Frame search
    uintptr_t GetVisibleFrameByChildOffsetAndChildCount(
        uint32_t childOffsetId, uint32_t minChildCount, uint32_t maxChildCount,
        uintptr_t excludeFrame = 0, uintptr_t excludeContext = 0);

    // Sorted child path navigation
    uintptr_t NavigateSortedChildPath(uintptr_t frame, const uint32_t* childIndices, uint32_t childIndexCount);

    // Text/numeric editing
    bool SetEditableTextValue(uintptr_t frame, const wchar_t* value, uintptr_t commitParentFrame = 0);
    bool SetEditableTextLocalOnly(uintptr_t frame, const wchar_t* value);
    bool SetNumericFrameValue(uintptr_t frame, uint32_t value, uintptr_t commitParentFrame = 0);
    bool SetNumericFrameLocalOnly(uintptr_t frame, uint32_t value);

    // Debug
    void DebugDumpChildFrames(uintptr_t frame, const char* label, uint32_t maxCount = 32);
    void DebugDumpFramesForContext(uintptr_t context, const char* label, uint32_t maxCount = 64);
    void DebugDumpVisibleFramesByChildOffset(uint32_t childOffsetId, const char* label, uint32_t maxCount = 64);

    // Key simulation
    bool KeyPress(uintptr_t frame, uint32_t vkCode);
    bool HasControlActionKeypress();
    bool ControlActionKeyDown(ControlAction action);
    bool ControlActionKeyUp(ControlAction action);
    bool ControlActionKeyPress(ControlAction action);
    bool ActionKeyDown(uint32_t action);
    bool ActionKeyUp(uint32_t action);
    bool ActionKeyPress(uint32_t action);

    // BotsHub-style "PerformAction" — single toggle call with the
    // BotsHub-compatible packet layout (ACTION_STRUCT = { action,
    // flag, type }). Used by UI toggle actions (quest log, map, etc.)
    // which need:
    //   - context = *(ActionBase + 0xC) + 0xA8   (type == 0 path)
    //   - arg shape: flag, &action_dword, 0     (not the 3-dword
    //     ControlActionPacket used by ControlActionKey*)
    //   - flag = 0x20 (CONTROL_TYPE_ACTIVATE) for a single press
    // See BotsHub-latest/lib/GWA2_Assembly.au3 1320 + 1643 for the
    // reference implementation.
    bool PerformUiAction(uint32_t action);
    bool PerformUiActionDirect(uint32_t action);

    // Mouse action testing
    bool TestMouseClickAction(uintptr_t frame, uint32_t currentState, uint32_t wparam, uint32_t lparam);
    bool TestMouseAction(uintptr_t frame, uint32_t currentState, uint32_t wparam, uint32_t lparam);

    // Low-level UIMessage dispatch
    void SendUIMessageAsm(uint32_t msgId, void* wParam, void* lParam);

    // Button click
    // Default path uses MouseUp(0x7); trade experiments can opt into MouseClick(0x8).
    bool ButtonClickImmediate(uintptr_t frame);
    bool ButtonClickImmediateFull(uintptr_t frame);
    bool ButtonClick(uintptr_t frame);
    bool ButtonClickMouseClick(uintptr_t frame);
    bool ButtonClickFullMouseClick(uintptr_t frame);
    bool ButtonClickFull(uintptr_t frame);
    bool ButtonClickByHash(uint32_t hash);

} // namespace GWA3::UIMgr

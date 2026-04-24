#pragma once

#include <gwa3/core/DialogHook.h>
#include <gwa3/managers/AgentMgr.h>

#include <cstdint>

namespace GWA3::DialogMgr {

    // StoC packet headers for dialog content
    constexpr uint32_t SMSG_DIALOG_BUTTON = 0x007E;  // 126
    constexpr uint32_t SMSG_DIALOG_BODY   = 0x0080;  // 128
    constexpr uint32_t SMSG_DIALOG_SENDER = 0x0081;  // 129

    // Dialog button info (stored from StoC packets)
    struct DialogButton {
        uint32_t dialog_id;
        uint32_t button_icon;
        uint32_t skill_id;
        wchar_t  label[128];
    };

    // Initialize: register StoC packet callbacks for dialog content.
    // Must be called after StoC::Initialize().
    bool Initialize();

    // Shutdown: remove callbacks.
    void Shutdown();

    // --- Query current dialog state ---

    // Returns true if a dialog is currently open (body text received).
    bool IsDialogOpen();

    // Get the NPC agent ID that sent the dialog (0 if no dialog open).
    uint32_t GetDialogSenderAgentId();

    // Get the dialog body text as a wide string (raw encoded from server).
    const wchar_t* GetDialogBodyRaw();

    // Get the decoded (readable) dialog body text.
    // Returns empty string if not yet decoded or no dialog open.
    const wchar_t* GetDialogBodyDecoded();

    // Get the number of dialog buttons currently available.
    uint32_t GetButtonCount();

    // Get a dialog button by index (0-based). Returns nullptr if out of range.
    const DialogButton* GetButton(uint32_t index);

    // Clear the current dialog state (called when dialog closes).
    void ClearDialog();

    // AutoIt-style live dialog hook helpers.
    void StartUIHook(uint32_t messageId = DialogHook::UIMSG_DIALOG);
    bool EndUIHook(uint32_t messageId, uint32_t timeoutMs = 2000u);
    bool WaitForUIMessage(uint32_t messageId, uint32_t timeoutMs = 2000u);
    bool WaitForDialogUIMessage(uint32_t timeoutMs = 2000u);
    uint32_t GetLastUIMessageId();
    uint32_t GetArmedUIMessageId();
    uint32_t GetObservedUIMessageId();
    uint32_t GetLastDialogId();
    void ResetHookState();
    void ResetRecentUITrace();
    uint32_t GetRecentUITrace(uint32_t* outMessages, uint32_t maxCount);

    // AutoIt-faithful dialog helpers.
    void GoNPC(uint32_t agentId);
    bool NPCHook(uint32_t agentId, uint32_t timeoutMs = 2000u);
    bool NPCHookEx(uint32_t agentId, AgentMgr::NpcInteractMode mode, uint32_t timeoutMs = 2000u);
    bool DialogHook(uint32_t dialogId, uint32_t timeoutMs = 2000u);

} // namespace GWA3::DialogMgr

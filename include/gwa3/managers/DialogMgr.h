#pragma once

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
    bool GetButton(uint32_t index, DialogButton& out);

    // Clear the current dialog state (called when dialog closes).
    void ClearDialog();

} // namespace GWA3::DialogMgr

#include <gwa3/managers/DialogMgr.h>
#include <gwa3/core/DialogHook.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/QuestMgr.h>
#include <gwa3/managers/StoCMgr.h>
#include <gwa3/utils/StringEncoding.h>
#include <gwa3/core/Log.h>
#include <gwa3/packets/CtoS.h>
#include <gwa3/packets/Headers.h>

#include <mutex>
#include <vector>
#include <cstring>

namespace GWA3::DialogMgr {

    #pragma pack(push, 1)
    struct StoC_DialogButton {
        uint32_t header;
        uint32_t button_icon;
        wchar_t  message[128];
        uint32_t dialog_id;
        uint32_t skill_id;
    };

    struct StoC_DialogBody {
        uint32_t header;
        wchar_t  message[122];
    };

    struct StoC_DialogSender {
        uint32_t header;
        uint32_t agent_id;
    };
    #pragma pack(pop)

    static std::mutex g_mutex;
    static bool g_dialogOpen = false;
    static uint32_t g_senderAgentId = 0;
    static wchar_t g_bodyRaw[256] = {};
    static wchar_t g_bodyDecoded[512] = {};
    static std::vector<DialogButton> g_buttons;

    static StoC::HookEntry g_hookBody;
    static StoC::HookEntry g_hookButton;
    static StoC::HookEntry g_hookSender;

    static bool TryDecode(const wchar_t* encoded, wchar_t* outBuf, uint32_t outSize) {
        outBuf[0] = L'\0';
        if (!encoded || !encoded[0]) return false;

        // Dungeon quest flows only depend on sender/button ids, not decoded text.
        // The current async GW decode path crashes when dialog body/button packets
        // arrive, so skip decode for encoded strings here.
        if (StringEncoding::IsValidEncStr(encoded)) {
            return false;
        }

        wcsncpy_s(outBuf, outSize, encoded, outSize - 1);
        return true;
    }

    static void OnDialogBody(StoC::HookStatus*, StoC::PacketBase* packet) {
        auto* p = reinterpret_cast<StoC_DialogBody*>(packet);
        std::lock_guard<std::mutex> lock(g_mutex);

        if (p->message[0] == L'\0') {
            g_dialogOpen = false;
            g_bodyRaw[0] = L'\0';
            g_bodyDecoded[0] = L'\0';
            g_buttons.clear();
            g_senderAgentId = 0;
            GWA3::Log::Info("[DialogMgr] Dialog closed");
            return;
        }

        g_dialogOpen = true;
        g_buttons.clear();
        wcsncpy_s(g_bodyRaw, p->message, 255);
        TryDecode(p->message, g_bodyDecoded, 512);

        GWA3::Log::Info("[DialogMgr] Dialog body received (%u raw chars, decoded=%u chars)",
                        static_cast<uint32_t>(wcslen(g_bodyRaw)),
                        static_cast<uint32_t>(wcslen(g_bodyDecoded)));
    }

    static void OnDialogButton(StoC::HookStatus*, StoC::PacketBase* packet) {
        auto* p = reinterpret_cast<StoC_DialogButton*>(packet);
        std::lock_guard<std::mutex> lock(g_mutex);

        DialogButton btn = {};
        btn.dialog_id = p->dialog_id;
        btn.button_icon = p->button_icon;
        btn.skill_id = p->skill_id;
        wcsncpy_s(btn.label, p->message, 127);

        wchar_t decoded[128] = {};
        if (TryDecode(p->message, decoded, 128) && decoded[0]) {
            wcsncpy_s(btn.label, decoded, 127);
        }

        g_buttons.push_back(btn);

        GWA3::Log::Info("[DialogMgr] Button added: dialog_id=0x%X icon=%u",
                        btn.dialog_id, btn.button_icon);
    }

    static void OnDialogSender(StoC::HookStatus*, StoC::PacketBase* packet) {
        auto* p = reinterpret_cast<StoC_DialogSender*>(packet);
        std::lock_guard<std::mutex> lock(g_mutex);
        g_senderAgentId = p->agent_id;
        GWA3::Log::Info("[DialogMgr] Dialog sender: agent_id=%u", p->agent_id);
    }

    bool Initialize() {
        bool ok = true;
        ok &= StoC::RegisterPostPacketCallback(&g_hookBody, SMSG_DIALOG_BODY, OnDialogBody);
        ok &= StoC::RegisterPostPacketCallback(&g_hookButton, SMSG_DIALOG_BUTTON, OnDialogButton);
        ok &= StoC::RegisterPostPacketCallback(&g_hookSender, SMSG_DIALOG_SENDER, OnDialogSender);

        if (ok) {
            GWA3::Log::Info("[DialogMgr] Initialized - listening for dialog packets");
        } else {
            GWA3::Log::Warn("[DialogMgr] Some packet callbacks failed to register");
        }
        return ok;
    }

    void Shutdown() {
        StoC::RemoveCallbacks(&g_hookBody);
        StoC::RemoveCallbacks(&g_hookButton);
        StoC::RemoveCallbacks(&g_hookSender);
        ClearDialog();
        GWA3::Log::Info("[DialogMgr] Shutdown");
    }

    bool IsDialogOpen() {
        std::lock_guard<std::mutex> lock(g_mutex);
        return g_dialogOpen;
    }

    uint32_t GetDialogSenderAgentId() {
        std::lock_guard<std::mutex> lock(g_mutex);
        return g_senderAgentId;
    }

    const wchar_t* GetDialogBodyRaw() {
        return g_bodyRaw;
    }

    const wchar_t* GetDialogBodyDecoded() {
        return g_bodyDecoded;
    }

    uint32_t GetButtonCount() {
        std::lock_guard<std::mutex> lock(g_mutex);
        return static_cast<uint32_t>(g_buttons.size());
    }

    const DialogButton* GetButton(uint32_t index) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (index >= g_buttons.size()) return nullptr;
        return &g_buttons[index];
    }

    void ClearDialog() {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_dialogOpen = false;
        g_senderAgentId = 0;
        g_bodyRaw[0] = L'\0';
        g_bodyDecoded[0] = L'\0';
        g_buttons.clear();
    }

    void StartUIHook(uint32_t messageId) {
        DialogHook::StartUIHook(messageId);
    }

    bool EndUIHook(uint32_t messageId, uint32_t timeoutMs) {
        return DialogHook::EndUIHook(messageId, timeoutMs);
    }

    bool WaitForUIMessage(uint32_t messageId, uint32_t timeoutMs) {
        return DialogHook::WaitForUIMessage(messageId, timeoutMs);
    }

    bool WaitForDialogUIMessage(uint32_t timeoutMs) {
        return DialogHook::WaitForDialogUIMessage(timeoutMs);
    }

    uint32_t GetLastUIMessageId() {
        return DialogHook::GetLastUIMessageId();
    }

    uint32_t GetArmedUIMessageId() {
        return DialogHook::GetArmedUIMessageId();
    }

    uint32_t GetObservedUIMessageId() {
        return DialogHook::GetObservedUIMessageId();
    }

    uint32_t GetLastDialogId() {
        return DialogHook::GetLastDialogId();
    }

    void ResetHookState() {
        DialogHook::Reset();
    }

    void ResetRecentUITrace() {
        DialogHook::ResetRecentUITrace();
    }

    uint32_t GetRecentUITrace(uint32_t* outMessages, uint32_t maxCount) {
        return DialogHook::GetRecentUITrace(outMessages, maxCount);
    }

    void GoNPC(uint32_t agentId) {
        CtoS::SendPacket(3, Packets::INTERACT_NPC, agentId, 0u);
    }

    bool NPCHook(uint32_t agentId, uint32_t timeoutMs) {
        return NPCHookEx(agentId, AgentMgr::NpcInteractMode::NativePostCallTarget, timeoutMs);
    }

    bool NPCHookEx(uint32_t agentId, AgentMgr::NpcInteractMode mode, uint32_t timeoutMs) {
        StartUIHook(DialogHook::UIMSG_DIALOG);
        AgentMgr::InteractNPCEx(agentId, mode);
        return EndUIHook(DialogHook::UIMSG_DIALOG, timeoutMs);
    }

    bool DialogHook(uint32_t dialogId, uint32_t timeoutMs) {
        StartUIHook(DialogHook::UIMSG_DIALOG);
        QuestMgr::Dialog(dialogId);
        return EndUIHook(DialogHook::UIMSG_DIALOG, timeoutMs);
    }

} // namespace GWA3::DialogMgr

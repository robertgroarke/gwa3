#include <gwa3/managers/DialogMgr.h>
#include <gwa3/managers/StoCMgr.h>
#include <gwa3/core/Log.h>

#include <mutex>
#include <vector>
#include <cstring>

namespace GWA3::DialogMgr {

    // StoC packet structures (match GWCA definitions)
    #pragma pack(push, 1)
    struct StoC_DialogButton {
        uint32_t header;        // 0x007E
        uint32_t button_icon;
        wchar_t  message[128];
        uint32_t dialog_id;
        uint32_t skill_id;
    };

    struct StoC_DialogBody {
        uint32_t header;        // 0x0080
        wchar_t  message[122];
    };

    struct StoC_DialogSender {
        uint32_t header;        // 0x0081
        uint32_t agent_id;
    };
    #pragma pack(pop)

    static std::mutex g_mutex;
    static bool g_dialogOpen = false;
    static uint32_t g_senderAgentId = 0;
    static wchar_t g_bodyRaw[256] = {};
    static std::vector<DialogButton> g_buttons;

    static StoC::HookEntry g_hookBody;
    static StoC::HookEntry g_hookButton;
    static StoC::HookEntry g_hookSender;

    static void OnDialogBody(StoC::HookStatus*, StoC::PacketBase* packet) {
        auto* p = reinterpret_cast<StoC_DialogBody*>(packet);
        std::lock_guard<std::mutex> lock(g_mutex);

        if (p->message[0] == L'\0') {
            // Empty body = dialog closed by server
            g_dialogOpen = false;
            g_bodyRaw[0] = L'\0';
            g_buttons.clear();
            g_senderAgentId = 0;
            GWA3::Log::Info("[DialogMgr] Dialog closed");
            return;
        }

        // New dialog opened — clear old state
        g_dialogOpen = true;
        g_buttons.clear();
        wcsncpy_s(g_bodyRaw, p->message, 255);

        GWA3::Log::Info("[DialogMgr] Dialog body received (%u chars)",
                        static_cast<uint32_t>(wcslen(g_bodyRaw)));
    }

    static void OnDialogButton(StoC::HookStatus*, StoC::PacketBase* packet) {
        auto* p = reinterpret_cast<StoC_DialogButton*>(packet);
        std::lock_guard<std::mutex> lock(g_mutex);

        DialogButton btn = {};
        btn.dialog_id = p->dialog_id;
        btn.button_icon = p->button_icon;
        btn.skill_id = p->skill_id;
        wcsncpy_s(btn.label, p->message, 127);

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
            GWA3::Log::Info("[DialogMgr] Initialized — listening for dialog packets");
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
        // Note: caller should hold awareness that this may change.
        // For snapshot serialization this is fine (single reader).
        return g_bodyRaw;
    }

    uint32_t GetButtonCount() {
        std::lock_guard<std::mutex> lock(g_mutex);
        return static_cast<uint32_t>(g_buttons.size());
    }

    bool GetButton(uint32_t index, DialogButton& out) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (index >= g_buttons.size()) return false;
        out = g_buttons[index];
        return true;
    }

    void ClearDialog() {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_dialogOpen = false;
        g_senderAgentId = 0;
        g_bodyRaw[0] = L'\0';
        g_buttons.clear();
    }

} // namespace GWA3::DialogMgr


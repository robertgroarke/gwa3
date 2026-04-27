#pragma once

#include <cstdint>

extern "C" void GWA3BotshubCommandReturnThunk();

namespace GWA3::CtoS {

    // Initialize: resolve PacketSend and PacketLocation from Offsets.
    bool Initialize();

    // Packet tap: lightweight header-frequency snapshot for diagnostics
    struct PacketTapSnapshot {
        uint32_t total_packets;
        uint32_t unique_headers;
        uint32_t headers[8];
        uint32_t counts[8];
    };

    PacketTapSnapshot GetPacketTapSnapshot();
    void ResetPacketTap();

    // ===== Game Command Queue =====
    // Enqueue a game command to execute in the Engine hook context.
    // This runs in the same hook point as AutoIt's command queue ---
    // the correct context for operations like Salvage that need
    // the game's internal state.
    typedef void (*GameCommandFn)(void* params);
    bool EnqueueGameCommand(GameCommandFn fn, const void* params, size_t paramSize);
    bool EnqueueBotshubCommand(const void* slot, size_t slotSize);
    bool IsGameCommandQueueIdle();  // true when no pending game commands
    bool IsBotshubQueueIdle();  // true when no pending botshub commands
    void DumpBotshubQueueState(const char* label);
    bool IsBotshubCommandLaneAvailable();

    // Temporarily unhook/rehook the engine inline hook.
    // Use around map transitions to prevent stale-trampoline crashes.
    void SuspendEngineHook();
    void ResumeEngineHook();

    // Raw packet send. Enqueues on game thread if not already on it.
    // header is the packet opcode, followed by up to 10 dword params.
    void SendPacket(uint32_t size, uint32_t header, ...);

    // Botshub packet send: queues a raw packet onto the engine command lane,
    // matching the legacy AutoIt transport more closely than the GameThread
    // pre/post-dispatch paths.
    bool SendPacketBotshub(uint32_t size, uint32_t header, ...);
    bool SendPacketBotshubHooked(uint32_t size, uint32_t header, ...);
    bool SendPacketGameCommandRaw(uint32_t size, uint32_t header, ...);
    bool SalvageItemBotshub(uint32_t itemId, uint32_t kitId, uint32_t sessionId);

    // Direct packet send: calls PacketSend immediately on the CURRENT thread,
    // bypassing GameThread::Enqueue and the engine hook detour entirely.
    // Use for packets that crash through the engine hook (e.g. INTERACT_NPC 0x39).
    // The MinHook tap still fires for logging but no detour/trampoline is involved.
void SendPacketDirect(uint32_t size, uint32_t header, ...);
void SendPacketDirectRaw(uint32_t size, uint32_t header, ...);

    // Sender-thread packet send: queues onto the dedicated packet sender thread
    // instead of the GameThread pre/post queues. Use when the GameThread lane
    // is wedged but PacketSend still needs to run between frames.
    void SendPacketThreaded(uint32_t size, uint32_t header, ...);

    // --- Type-safe wrappers ---

    void MoveToCoord(float x, float y);
    void Dialog(uint32_t dialogId);
    void ChangeTarget(uint32_t agentId);
    void ActionAttack(uint32_t agentId, uint32_t callTarget = 0);
    void CancelAction();

    // Map travel: id = MapID, region/district/lang per GW protocol
    void MapTravel(uint32_t mapId, uint32_t region = 0, uint32_t district = 0, uint32_t language = 0);

    // Hero commands
    void HeroAdd(uint32_t heroId);
    void HeroKick(uint32_t heroId);
    void HeroBehavior(uint32_t heroIndex, uint32_t behavior);
    void HeroFlagSingle(uint32_t heroIndex, float x, float y);
    void HeroFlagAll(float x, float y);

    // Item commands
    void UseItem(uint32_t itemId);
    void EquipItem(uint32_t itemId);
    void DropItem(uint32_t itemId);
    void PickUpItem(uint32_t itemAgentId);
    void MoveItem(uint32_t itemId, uint32_t bagId, uint32_t slot);

    // Quest
    void QuestAbandon(uint32_t questId);
    void QuestSetActive(uint32_t questId);

    // Skill
    // Packet form uses the resolved skill id, not the skillbar slot.
    void UseSkill(uint32_t skillId, uint32_t targetAgentId, uint32_t callTarget = 0);

    // Trade
    void TradeOfferItem(uint32_t itemId, uint32_t quantity);
    bool TradeOfferItemBotshub(uint32_t itemId, uint32_t quantity);
    void TradeCancel();
    void TradeAccept();
    void TradeCancelThreaded();
    void TradeAcceptThreaded();

} // namespace GWA3::CtoS

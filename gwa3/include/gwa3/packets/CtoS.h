#pragma once

#include <cstdint>

namespace GWA3::CtoS {

    // Initialize: resolve PacketSend and PacketLocation from Offsets.
    bool Initialize();

    // Raw packet send. Enqueues on game thread if not already on it.
    // header is the packet opcode, followed by up to 10 dword params.
    void SendPacket(uint32_t size, uint32_t header, ...);

    // --- Type-safe wrappers ---

    void MoveToCoord(float x, float y);
    void Dialog(uint32_t dialogId);
    void ChangeTarget(uint32_t agentId);
    void AttackAgent(uint32_t agentId);
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
    void UseSkill(uint32_t skillSlot, uint32_t targetAgentId, uint32_t callTarget = 0);
    void SwitchWeaponSet(uint32_t setIndex);

    // Trade
    void TradePlayer(uint32_t agentId);
    void TradeCancel();
    void TradeAccept();

} // namespace GWA3::CtoS

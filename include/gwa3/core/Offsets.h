#pragma once

#include <cstdint>

namespace GWA3::Offsets {

    // Priority levels: P0 = required for bot operation, P1 = important, P2 = optional
    enum class Priority { P0, P1, P2 };

    // Pattern types
    enum class PatternType { Ptr, Func, Hook };

    // Resolve all registered scan patterns via the Scanner.
    // Returns false if any P0 or P1 pattern fails.
    bool ResolveAll();
    bool RefreshBasePointer();

    // Get count of resolved/failed patterns.
    int GetResolvedCount();
    int GetFailedCount();

    // ===== Core =====
    extern uintptr_t BasePointer;
    extern uintptr_t Ping;
    extern uintptr_t StatusCode;
    extern uintptr_t PacketSend;         // func
    extern uintptr_t PacketLocation;
    extern uintptr_t Action;             // func
    extern uintptr_t ActionBase;
    extern uintptr_t Environment;
    extern uintptr_t PreGame;            // assertion-based
    extern uintptr_t FrameArray;         // assertion-based
    extern uintptr_t SceneContext;

    // ===== Skills =====
    extern uintptr_t SkillBase;
    extern uintptr_t SkillTimer;
    extern uintptr_t UseSkill;           // func
    extern uintptr_t UseHeroSkill;       // func

    // ===== Friends =====
    extern uintptr_t FriendList;         // assertion-based
    extern uintptr_t PlayerStatus;       // func
    extern uintptr_t AddFriend;          // func
    extern uintptr_t RemoveFriend;       // func

    // ===== Attributes =====
    extern uintptr_t AttributeInfo;
    extern uintptr_t IncreaseAttribute;  // func
    extern uintptr_t DecreaseAttribute;  // func

    // ===== Trade =====
    extern uintptr_t Transaction;        // func
    extern uintptr_t BuyItemBase;
    extern uintptr_t RequestQuote;       // func
    extern uintptr_t Salvage;            // func - Reforged local salvage init (0x66 wrapper)
    extern uintptr_t SalvageSessionOpen; // func - native 0x77 wrapper
    extern uintptr_t SalvageSessionCancel; // func - native 0x78 wrapper
    extern uintptr_t SalvageSessionDone; // func - native 0x79 wrapper
    extern uintptr_t SalvageMaterials;   // func - native 0x7A wrapper
    extern uintptr_t SalvageUpgrade;     // func - native 0x7B wrapper
    extern uintptr_t SalvageGlobal;

    // ===== Agents =====
    extern uintptr_t AgentBase;
    extern uintptr_t ChangeTarget;       // func
    extern uintptr_t CurrentTarget;
    extern uintptr_t TargetLog;          // hook seam
    extern uintptr_t MyID;

    // ===== Map =====
    extern uintptr_t Move;               // func
    extern uintptr_t ClickCoords;
    extern uintptr_t InstanceInfo;
    extern uintptr_t WorldConst;
    extern uintptr_t Region;
    extern uintptr_t AreaInfo;

    // ===== Trade UI =====
    extern uintptr_t TradeCancel;        // func

    // ===== UI =====
    extern uintptr_t UIMessage;          // func
    extern uintptr_t CompassFlag;        // func
    extern uintptr_t PartySearchButtonCallback; // func
    extern uintptr_t PartyWindowButtonCallback; // func
    extern uintptr_t EnterMission;       // func
    extern uintptr_t SetDifficulty;      // func
    extern uintptr_t OpenChest;          // func
    extern uintptr_t Dialog;             // func
    extern uintptr_t AiMode;
    extern uintptr_t HeroCommand;
    extern uintptr_t HeroSkills;

    // ===== Party =====
    extern uintptr_t PlayerAdd;          // assertion-based
    extern uintptr_t PlayerKick;         // assertion-based
    extern uintptr_t PartyInvitations;

    // ===== Quest =====
    extern uintptr_t ActiveQuest;

    // ===== Hooks =====
    extern uintptr_t Engine;             // hook
    extern uintptr_t Render;             // hook
    extern uintptr_t LoadFinished;       // hook
    extern uintptr_t Trader;             // hook
    extern uintptr_t TradePartner;       // hook

    // ===== Text =====
    extern uintptr_t ValidateAsyncDecodeStr;     // assertion-based, func
    extern uintptr_t ValidateAsyncDecodeStrGwca; // GWCA byte-pattern scan, func

    // ===== Chat =====
    extern uintptr_t PostMessage;
    extern uintptr_t ChatLog;            // hook

    // ===== Titles =====
    extern uintptr_t TitleClientDataBase; // ptr — static TitleClientData[] in .rdata

    // ===== Camera =====
    extern uintptr_t CameraClass;        // ptr — Camera struct global
    extern uintptr_t FogPatch;           // ptr — fog render instruction to patch

    // ===== Effects =====
    extern uintptr_t PostProcessEffect;  // func — visual post-process effect
    extern uintptr_t DropBuff;           // func — drop maintained enchantment

    // ===== Render =====
    extern uintptr_t GwEndScene;         // func — GW render end-scene entry

    // ===== Items =====
    extern uintptr_t ItemClick;          // func — item interaction dispatch

    // ===== Memory Patches =====
    extern uintptr_t CameraUpdateBypass; // ptr — camera interpolation copy-back instruction
    extern uintptr_t LevelDataBypass;    // ptr — JZ instruction for level-data validation
    extern uintptr_t MapPortBypass;      // ptr — JNZ instruction for map/port validation

    // ===== Agent Interaction (GWCA) =====
    extern uintptr_t InteractAgent;      // func — interaction dispatcher (resolves CallTarget at +0xD6)
    extern uintptr_t WorldActionFunc;    // func — native WorldAction(action_id, agent_id, call_target)
    extern uintptr_t CallTargetFunc;     // func — native CallTarget(type, agent_id)
    extern uintptr_t InteractNPCFunc;    // func — native InteractNPC(agent_id, call_target)

    // ===== Trade (GWCA) =====
    extern uintptr_t OfferTradeItem;     // func — __fastcall offer item in trade window
    extern uintptr_t UpdateTradeCart;    // func — trade cart update (captures window context)
    extern uintptr_t TradeHackPatch;     // ptr — upstream ToggleTradePatch target (55 <-> C3)
    extern uintptr_t TradeSendOffer;     // func — submit current trade offer / offered gold
    extern uintptr_t TradeCancelOffer;   // func — retract submitted offer to modify it
    extern uintptr_t TradeAcceptOffer;   // func — accept current player trade
    extern uintptr_t TradeRemoveItem;    // func — remove offered item by slot

    // ===== Chat (GWCA) =====
    extern uintptr_t SendChatFunc;       // func — native chat send
    extern uintptr_t AddToChatLog;       // func — native chat log write

    // ===== Map (GWCA) =====
    extern uintptr_t SkipCinematicFunc;  // func — native cinematic skip

    // ===== Quest (GWCA) =====
    extern uintptr_t RequestQuestInfo;   // func — quest info request

    // ===== FriendList (GWCA) =====
    extern uintptr_t FriendListAddr;     // ptr — FriendList struct base
    extern uintptr_t FriendEventHandler; // func — friend status event handler

    // ===== Compass (GWCA) =====
    extern uintptr_t DrawOnCompass;      // func — compass drawing callback

    // ===== Chat Colors (GWCA) =====
    extern uintptr_t GetSenderColor;     // func — chat sender name color
    extern uintptr_t GetMessageColor;    // func — chat message body color

    // ===== Frame UI (GWA3-new) =====
    extern uintptr_t SendFrameUIMsg;     // func — scanned via pattern 83 C1 DC E8

    // ===== Centralized Context Resolution =====
    // These follow the standard GW pointer chain from BasePointer.
    // SEH-protected; return 0/nullptr on any fault.

    // GameContext: *BasePointer -> +0x18
    uintptr_t ResolveGameContext();

    // WorldContext: *BasePointer -> +0x18 -> +0x2C
    uintptr_t ResolveWorldContext();

} // namespace GWA3::Offsets

// GWA3-044: Compile-time verification of all packet header constants.
// Every value is checked against the canonical GWA2_Headers.au3 source.
// If any constant is wrong, the build fails with a clear error message.

#include <gwa3/packets/Headers.h>
#include <gwa3/testing/TestFramework.h>

namespace P = GWA3::Packets;

// ===== Trade (Player) =====
static_assert(P::TRADE_PLAYER                      == 0x00, "TRADE_PLAYER");
static_assert(P::TRADE_CANCEL                      == 0x01, "TRADE_CANCEL");
static_assert(P::TRADE_ADD_ITEM                    == 0x02, "TRADE_ADD_ITEM");
static_assert(P::TRADE_SUBMIT_OFFER                == 0x03, "TRADE_SUBMIT_OFFER");
static_assert(P::TRADE_OFFER_ITEM                  == 0x04, "TRADE_OFFER_ITEM");
static_assert(P::TRADE_REMOVE_ITEM                 == 0x05, "TRADE_REMOVE_ITEM");
static_assert(P::TRADE_CHANGE_OFFER                == 0x06, "TRADE_CHANGE_OFFER");
static_assert(P::TRADE_ACCEPT                      == 0x07, "TRADE_ACCEPT");
static_assert(P::TRADE_INITIATE                    == 0x49, "TRADE_INITIATE");
static_assert(P::TRANSACT_ITEMS                    == 0x4D, "TRANSACT_ITEMS");

// ===== Trade (NPC) =====
static_assert(P::BUY_MATERIALS                     == 0x4A, "BUY_MATERIALS");
static_assert(P::REQUEST_QUOTE                     == 0x4C, "REQUEST_QUOTE");

// ===== Connection & Ping =====
static_assert(P::DISCONNECT                        == 0x08, "DISCONNECT");
static_assert(P::PING_REPLY                        == 0x09, "PING_REPLY");
static_assert(P::HEARTBEAT                         == 0x0A, "HEARTBEAT");
static_assert(P::PING_REQUEST                      == 0x0B, "PING_REQUEST");

// ===== Quests =====
static_assert(P::QUEST_ABANDON                     == 0x11, "QUEST_ABANDON");
static_assert(P::QUEST_REQUEST_INFOS               == 0x12, "QUEST_REQUEST_INFOS");
static_assert(P::QUEST_SET_ACTIVE                  == 0x14, "QUEST_SET_ACTIVE");
static_assert(P::DIALOG_SEND                       == 0x3B, "DIALOG_SEND");

// ===== Heroes & NPCs =====
static_assert(P::HERO_BEHAVIOR                     == 0x15, "HERO_BEHAVIOR");
static_assert(P::HERO_LOCK_TARGET                  == 0x16, "HERO_LOCK_TARGET");
static_assert(P::HERO_SKILL_TOGGLE                 == 0x19, "HERO_SKILL_TOGGLE");
static_assert(P::HERO_FLAG_SINGLE                  == 0x1A, "HERO_FLAG_SINGLE");
static_assert(P::HERO_FLAG_ALL                     == 0x1B, "HERO_FLAG_ALL");
static_assert(P::USE_HERO_SKILL                    == 0x1C, "USE_HERO_SKILL");
static_assert(P::HERO_ADD                          == 0x1E, "HERO_ADD");
static_assert(P::HERO_KICK                         == 0x1F, "HERO_KICK");

// ===== Combat & Targeting =====
static_assert(P::CALL_TARGET                       == 0x23, "CALL_TARGET");
static_assert(P::ATTACK_AGENT                      == 0x24, "ATTACK_AGENT");
static_assert(P::ACTION_ATTACK                     == 0x26, "ACTION_ATTACK");
static_assert(P::ACTION_CANCEL                     == 0x28, "ACTION_CANCEL");
static_assert(P::TARGET_AGENT                      == 0xC1, "TARGET_AGENT");

// ===== Movement & Interaction =====
static_assert(P::DRAW_MAP                          == 0x2B, "DRAW_MAP");
static_assert(P::INTERACT_PLAYER                   == 0x33, "INTERACT_PLAYER");
static_assert(P::INTERACT_NPC                      == 0x39, "INTERACT_NPC");
static_assert(P::MOVE_TO_COORD                     == 0x3E, "MOVE_TO_COORD");
static_assert(P::ITEM_INTERACT                     == 0x3F, "ITEM_INTERACT");
static_assert(P::ROTATE_PLAYER                     == 0x40, "ROTATE_PLAYER");

// ===== Inventory & Items =====
static_assert(P::DROP_ITEM                         == 0x2C, "DROP_ITEM");
static_assert(P::DROP_GOLD                         == 0x2F, "DROP_GOLD");
static_assert(P::ITEM_EQUIP                        == 0x30, "ITEM_EQUIP");
static_assert(P::SWITCH_SET                        == 0x32, "SWITCH_SET");
static_assert(P::SKILL_EQUIP                       == 0x3C, "SKILL_EQUIP");
static_assert(P::UNEQUIP_ITEM                      == 0x4F, "UNEQUIP_ITEM");
static_assert(P::SIGNPOST_RUN                      == 0x51, "SIGNPOST_RUN");
static_assert(P::EQUIP_VISIBILITY                  == 0x56, "EQUIP_VISIBILITY");
static_assert(P::ITEM_DESTROY                      == 0x69, "ITEM_DESTROY");
static_assert(P::EQUIP_BAG                         == 0x6B, "EQUIP_BAG");
static_assert(P::TOME_UNLOCK_SKILL                 == 0x6D, "TOME_UNLOCK_SKILL");
static_assert(P::ITEM_APPLY_DYE                    == 0x70, "ITEM_APPLY_DYE");
static_assert(P::ITEM_MOVE                         == 0x72, "ITEM_MOVE");
static_assert(P::ITEMS_ACCEPT_UNCLAIMED            == 0x73, "ITEMS_ACCEPT_UNCLAIMED");
static_assert(P::ITEM_SPLIT_STACK                  == 0x75, "ITEM_SPLIT_STACK");
static_assert(P::ITEM_USE                          == 0x7E, "ITEM_USE");

// ===== Identify & Salvage =====
static_assert(P::ITEM_IDENTIFY                     == 0x6C, "ITEM_IDENTIFY");
static_assert(P::SALVAGE_SESSION_OPEN              == 0x77, "SALVAGE_SESSION_OPEN");
static_assert(P::SALVAGE_SESSION_CANCEL            == 0x78, "SALVAGE_SESSION_CANCEL");
static_assert(P::SALVAGE_SESSION_DONE              == 0x79, "SALVAGE_SESSION_DONE");
static_assert(P::SALVAGE_MATERIALS                 == 0x7A, "SALVAGE_MATERIALS");
static_assert(P::SALVAGE_UPGRADE                   == 0x7B, "SALVAGE_UPGRADE");

// ===== Gold =====
static_assert(P::CHANGE_GOLD                       == 0x7C, "CHANGE_GOLD");

// ===== Upgrade / Armor =====
static_assert(P::UPGRADE_SESSION_OPEN              == 0x80, "UPGRADE_SESSION_OPEN");
static_assert(P::UPGRADE_SESSION_CANCEL            == 0x81, "UPGRADE_SESSION_CANCEL");
static_assert(P::UPGRADE_SESSION_VALID_UPGRADE     == 0x82, "UPGRADE_SESSION_VALID_UPGRADE");
static_assert(P::UPGRADE_ARMOR_1                   == 0x83, "UPGRADE_ARMOR_1");
static_assert(P::UPGRADE_ARMOR_2                   == 0x86, "UPGRADE_ARMOR_2");

// ===== Skills & Attributes =====
static_assert(P::ATTRIBUTE_DECREASE                == 0x0E, "ATTRIBUTE_DECREASE");
static_assert(P::ATTRIBUTE_INCREASE                == 0x0F, "ATTRIBUTE_INCREASE");
static_assert(P::ATTRIBUTE_LOAD                    == 0x10, "ATTRIBUTE_LOAD");
static_assert(P::PROFESSION_CHANGE                 == 0x41, "PROFESSION_CHANGE");
static_assert(P::USE_SKILL                         == 0x46, "USE_SKILL");
static_assert(P::SET_SKILLBAR_SKILL                == 0x5C, "SET_SKILLBAR_SKILL");
static_assert(P::LOAD_SKILLBAR                     == 0x5D, "LOAD_SKILLBAR");
static_assert(P::PLAYER_ATTR_SET                   == 0x98, "PLAYER_ATTR_SET");

// ===== Instance & Party =====
static_assert(P::INSTANCE_LOAD_REQUEST_SPAWN       == 0x87, "INSTANCE_LOAD_REQUEST_SPAWN");
static_assert(P::INSTANCE_LOAD_REQUEST_PLAYERS     == 0x8F, "INSTANCE_LOAD_REQUEST_PLAYERS");
static_assert(P::INSTANCE_LOAD_REQUEST_ITEMS       == 0x90, "INSTANCE_LOAD_REQUEST_ITEMS");
static_assert(P::SET_DIFFICULTY                     == 0x9B, "SET_DIFFICULTY");
static_assert(P::PARTY_ACCEPT_INVITE               == 0x9C, "PARTY_ACCEPT_INVITE");
static_assert(P::INVITE_CANCEL                     == 0x9D, "INVITE_CANCEL");
static_assert(P::PARTY_ACCEPT_REFUSE               == 0x9E, "PARTY_ACCEPT_REFUSE");
static_assert(P::PARTY_INVITE_NPC                  == 0x9F, "PARTY_INVITE_NPC");
static_assert(P::PARTY_INVITE_PLAYER               == 0xA0, "PARTY_INVITE_PLAYER");
static_assert(P::PARTY_INVITE_PLAYER_NAME          == 0xA1, "PARTY_INVITE_PLAYER_NAME");
static_assert(P::PARTY_LEAVE                       == 0xA2, "PARTY_LEAVE");
static_assert(P::PARTY_CANCEL_ENTER_CHALLENGE      == 0xA3, "PARTY_CANCEL_ENTER_CHALLENGE");
static_assert(P::PARTY_ENTER_CHALLENGE             == 0xA5, "PARTY_ENTER_CHALLENGE");
static_assert(P::PARTY_RETURN_TO_OUTPOST           == 0xA7, "PARTY_RETURN_TO_OUTPOST");
static_assert(P::PARTY_KICK_NPC                    == 0xA8, "PARTY_KICK_NPC");
static_assert(P::PARTY_KICK_PLAYER                 == 0xA9, "PARTY_KICK_PLAYER");
static_assert(P::PARTY_SEARCH_SEEK                 == 0xAA, "PARTY_SEARCH_SEEK");
static_assert(P::PARTY_SEARCH_CANCEL               == 0xAB, "PARTY_SEARCH_CANCEL");
static_assert(P::PARTY_SEARCH_REQUEST_JOIN         == 0xAC, "PARTY_SEARCH_REQUEST_JOIN");
static_assert(P::PARTY_ENTER_FOREIGN_MISSION       == 0xAD, "PARTY_ENTER_FOREIGN_MISSION");
static_assert(P::PARTY_SEARCH_TYPE                 == 0xAE, "PARTY_SEARCH_TYPE");
static_assert(P::PARTY_READY_STATUS                == 0xAF, "PARTY_READY_STATUS");

// ===== Travel =====
static_assert(P::GUILDHALL_TRAVEL                  == 0xB0, "GUILDHALL_TRAVEL");
static_assert(P::MAP_TRAVEL                        == 0xB1, "MAP_TRAVEL");
static_assert(P::GUILDHALL_LEAVE                   == 0xB2, "GUILDHALL_LEAVE");

// ===== Guild =====
static_assert(P::BUY_GUILD_CAPE                    == 0x38, "BUY_GUILD_CAPE");
static_assert(P::SHOW_HIDE                         == 0x57, "SHOW_HIDE");
static_assert(P::GUILD_SET_OFFICER                 == 0xBD, "GUILD_SET_OFFICER");
static_assert(P::GUILD_ANNOUCEMENTS                == 0xBE, "GUILD_ANNOUCEMENTS");

// ===== Chat =====
static_assert(P::SEND_CHAT                         == 0x64, "SEND_CHAT");

// ===== Titles =====
static_assert(P::TITLE_DISPLAY                     == 0x58, "TITLE_DISPLAY");
static_assert(P::TITLE_HIDE                        == 0x59, "TITLE_HIDE");
static_assert(P::HOM_DIALOG                        == 0x60, "HOM_DIALOG");
static_assert(P::TITLE_UPDATE                      == 0xF5, "TITLE_UPDATE");

// ===== Buffs =====
static_assert(P::BUFF_DROP                         == 0x29, "BUFF_DROP");

// ===== Faction =====
static_assert(P::FACTION_DEPOSIT                   == 0x35, "FACTION_DEPOSIT");

// ===== Misc =====
static_assert(P::CINEMATIC_SKIP                    == 0x63, "CINEMATIC_SKIP");
static_assert(P::OPEN_CHEST                        == 0x53, "OPEN_CHEST");

// Total: 113 static_assert checks — matches Headers.h count exactly.

// Runtime summary test (confirms this file compiled = all asserts passed)
GWA3_TEST(header_constants_compile_check, {
    // If we reach here, all 113 static_asserts passed at compile time.
    GWA3_ASSERT(true);
})

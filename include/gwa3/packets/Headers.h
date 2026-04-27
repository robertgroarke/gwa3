#pragma once
// Game packet header constants ported from AutoIt GWA2_Headers.au3
// Source: GWA Censured/lib/botshub/GWA2_Headers.au3 (v1.7, rheek/mhaendler/3vcloud/MrJambix/night)
// These are GAME_CMSG_ opcodes used for client-to-server packets.

#include <cstdint>

namespace GWA3::Packets {

// ===== Trade (Player) =====
constexpr uint32_t TRADE_PLAYER                       = 0x00;  // Receive trade request from player
constexpr uint32_t TRADE_CANCEL                       = 0x01;  // Cancels an ongoing trade
constexpr uint32_t TRADE_ADD_ITEM                     = 0x02;  // Adds an item to the trade window
constexpr uint32_t TRADE_SUBMIT_OFFER                 = 0x03;  // Submit offer
constexpr uint32_t TRADE_OFFER_ITEM                   = 0x04;  // Add item to trade window
constexpr uint32_t TRADE_REMOVE_ITEM                  = 0x05;  // Removes an item from the trade window
constexpr uint32_t TRADE_CHANGE_OFFER                 = 0x06;  // Cancels a previously sent trade offer
constexpr uint32_t TRADE_ACCEPT                       = 0x07;  // Accepts a trade offer
constexpr uint32_t TRADE_INITIATE                     = 0x49;  // Initiates a trade with another player
constexpr uint32_t TRANSACT_ITEMS                     = 0x4D;  // Confirms a transaction involving items

// ===== Trade (NPC) =====
constexpr uint32_t BUY_MATERIALS                      = 0x4A;  // Buy materials
constexpr uint32_t REQUEST_QUOTE                      = 0x4C;  // Requests a quote or price for an item

// ===== Connection & Ping =====
constexpr uint32_t DISCONNECT                         = 0x08;  // Disconnects from the server or session
constexpr uint32_t PING_REPLY                         = 0x09;  // Sends a reply to a ping request
constexpr uint32_t HEARTBEAT                          = 0x0A;  // Sends a heartbeat signal to maintain connection
constexpr uint32_t PING_REQUEST                       = 0x0B;  // Requests a ping to check the connection status

// ===== Quests =====
constexpr uint32_t QUEST_ABANDON                      = 0x11;  // Abandons a selected quest
constexpr uint32_t QUEST_REQUEST_INFOS                = 0x12;  // Requests information about a quest
constexpr uint32_t QUEST_SET_ACTIVE                   = 0x14;  // Confirms setting a quest as active
constexpr uint32_t DIALOG_SEND                        = 0x3B;  // Dialog header (non-living target variant)
constexpr uint32_t DIALOG_SEND_LIVING                 = 0x3A;  // Dialog header (living-agent target variant)

// ===== Heroes & NPCs =====
constexpr uint32_t HERO_BEHAVIOR                      = 0x15;  // Sets the behavior/aggression level of a hero
constexpr uint32_t HERO_LOCK_TARGET                   = 0x16;  // Locks a target for the hero
constexpr uint32_t HERO_SKILL_TOGGLE                  = 0x19;  // Toggles a hero's skill on or off
constexpr uint32_t HERO_FLAG_ALL                      = 0x1B;  // Sets or clears the party position flag
constexpr uint32_t HERO_FLAG_SINGLE                   = 0x1A;  // Sets or clears a single hero position flag
constexpr uint32_t USE_HERO_SKILL                     = 0x1C;  // For use with UseHeroSkillByPacket() only
constexpr uint32_t HERO_ADD                           = 0x1E;  // Adds a hero to the party
constexpr uint32_t HERO_KICK                          = 0x1F;  // Removes a hero or all heroes from the party

// ===== Combat & Targeting =====
constexpr uint32_t CALL_TARGET                        = 0x23;  // Calls the target without attacking (Ctrl+Shift+Space)
constexpr uint32_t ATTACK_AGENT                       = 0x24;  // Initiates an attack on a selected agent (I_HEADER variant)
constexpr uint32_t ACTION_ATTACK                      = 0x26;  // Initiates an attack on a selected agent
constexpr uint32_t ACTION_CANCEL                      = 0x28;  // Cancels the current action
constexpr uint32_t TARGET_AGENT                       = 0xC1;  // Target an agent

// ===== Movement & Interaction =====
constexpr uint32_t DRAW_MAP                           = 0x2B;  // Draws on the map (for map pinging/markers)
constexpr uint32_t INTERACT_PLAYER                    = 0x33;  // Follows the agent/npc. Ctrl+Click triggers 'I am following Person' in chat
constexpr uint32_t INTERACT_NPC                       = 0x39;  // NPC interact header (raw packet variant)
constexpr uint32_t INTERACT_LIVING                    = 0x38;  // Living-agent interact header (native variant)
constexpr uint32_t MOVE_TO_COORD                      = 0x3E;  // Moves to specified coordinates
constexpr uint32_t ITEM_INTERACT                      = 0x3F;  // Interacts with an item in the environment to pick it up or interact
constexpr uint32_t ROTATE_PLAYER                      = 0x40;  // Rotates the player character

// ===== Inventory & Items =====
constexpr uint32_t DROP_ITEM                          = 0x2C;  // Drops item from inventory to ground
constexpr uint32_t DROP_GOLD                          = 0x2F;  // Drops gold from inventory to ground
constexpr uint32_t ITEM_EQUIP                         = 0x30;  // Equips item from inventory/chest
constexpr uint32_t SWITCH_SET                         = 0x32;  // Switch weapon set
constexpr uint32_t SKILL_EQUIP                        = 0x3C;  // Equip Skill from NPC Dialog
constexpr uint32_t UNEQUIP_ITEM                       = 0x4F;  // Unequips an item
constexpr uint32_t INTERACT_GADGET                    = 0x50;  // Interacts with a gadget/signpost
constexpr uint32_t SIGNPOST_RUN                       = 0x51;  // Runs to signpost
constexpr uint32_t EQUIP_VISIBILITY                   = 0x56;  // Toggles the visibility of equipped items
constexpr uint32_t ITEM_DESTROY                       = 0x69;  // Destroys the item
constexpr uint32_t EQUIP_BAG                          = 0x6B;  // Equip bag
constexpr uint32_t TOME_UNLOCK_SKILL                  = 0x6D;  // Unlocks a skill using a tome
constexpr uint32_t ITEM_APPLY_DYE                     = 0x70;  // Applies dye to an item
constexpr uint32_t ITEM_MOVE                          = 0x72;  // Moves an item within the inventory / can equip bags
constexpr uint32_t ITEMS_ACCEPT_UNCLAIMED             = 0x73;  // Accepts items not picked up in missions
constexpr uint32_t ITEM_SPLIT_STACK                   = 0x75;  // Splits a stack of items
constexpr uint32_t ITEM_USE                           = 0x7E;  // Uses item from inventory/chest

// ===== Identify & Salvage =====
constexpr uint32_t ITEM_IDENTIFY                      = 0x6C;  // Identifies item in inventory
constexpr uint32_t SALVAGE_SESSION_OPEN               = 0x77;  // Start salvage session
constexpr uint32_t SALVAGE_SESSION_CANCEL             = 0x78;  // Cancel salvage session
constexpr uint32_t SALVAGE_SESSION_DONE               = 0x79;  // Finish salvage session
constexpr uint32_t SALVAGE_MATERIALS                  = 0x7A;  // Salvages materials from item
constexpr uint32_t SALVAGE_UPGRADE                    = 0x7B;  // Salvages mods from item

// ===== Gold =====
constexpr uint32_t CHANGE_GOLD                        = 0x7C;  // Moves gold (chest to inventory and vice versa)

// ===== Upgrade / Armor =====
constexpr uint32_t UPGRADE_SESSION_OPEN               = 0x80;  // Upgrade armor (not in use)
constexpr uint32_t UPGRADE_SESSION_CANCEL             = 0x81;  // Cancel upgrade command (not in use)
constexpr uint32_t UPGRADE_SESSION_VALID_UPGRADE      = 0x82;  // Validate upgrade session
constexpr uint32_t UPGRADE_ARMOR_1                    = 0x83;  // Upgrade armor step 1 (NOT TESTED)
constexpr uint32_t UPGRADE_ARMOR_2                    = 0x86;  // Upgrade armor step 2 (NOT TESTED)

// ===== Skills & Attributes =====
constexpr uint32_t ATTRIBUTE_DECREASE                 = 0x0E;  // Decreases attribute level
constexpr uint32_t ATTRIBUTE_INCREASE                 = 0x0F;  // Increases attribute level
constexpr uint32_t ATTRIBUTE_LOAD                     = 0x10;  // Load attribute level
constexpr uint32_t PROFESSION_CHANGE                  = 0x41;  // Changes secondary class (from Build window)
constexpr uint32_t USE_SKILL                          = 0x46;  // Use a skill
constexpr uint32_t SET_SKILLBAR_SKILL                 = 0x5C;  // Changes a skill on the skillbar
constexpr uint32_t LOAD_SKILLBAR                      = 0x5D;  // Loads a complete build
constexpr uint32_t PLAYER_ATTR_SET                    = 0x98;  // Set player attributes

// ===== Instance & Party =====
constexpr uint32_t INSTANCE_LOAD_REQUEST_SPAWN        = 0x87;  // Requests spawn in an instance
constexpr uint32_t INSTANCE_LOAD_REQUEST_PLAYERS      = 0x8F;  // Requests player information in an instance
constexpr uint32_t INSTANCE_LOAD_REQUEST_ITEMS        = 0x90;  // Requests item information in an instance
constexpr uint32_t SET_DIFFICULTY                      = 0x9B;  // Toggles hard- and normal mode
constexpr uint32_t PARTY_ACCEPT_INVITE                = 0x9C;  // Accepts a party invitation
constexpr uint32_t INVITE_CANCEL                      = 0x9D;  // Cancel invitation of player
constexpr uint32_t PARTY_ACCEPT_REFUSE                = 0x9E;  // Refuses a party invitation
constexpr uint32_t PARTY_INVITE_NPC                   = 0x9F;  // Adds henchman to party
constexpr uint32_t PARTY_INVITE_PLAYER                = 0xA0;  // Invite target player to party
constexpr uint32_t PARTY_INVITE_PLAYER_NAME           = 0xA1;  // Invites a player to the party by name
constexpr uint32_t PARTY_LEAVE                        = 0xA2;  // Leaves the party
constexpr uint32_t PARTY_CANCEL_ENTER_CHALLENGE       = 0xA3;  // Cancels entry into a challenge or mission
constexpr uint32_t PARTY_ENTER_CHALLENGE              = 0xA5;  // Enter a mission/challenge
constexpr uint32_t PARTY_RETURN_TO_OUTPOST            = 0xA7;  // Returns to outpost after /resign
constexpr uint32_t PARTY_KICK_NPC                     = 0xA8;  // Kicks a henchman from party
constexpr uint32_t PARTY_KICK_PLAYER                  = 0xA9;  // Kicks a player from the party
constexpr uint32_t PARTY_SEARCH_SEEK                  = 0xAA;  // Seeks members for party formation
constexpr uint32_t PARTY_SEARCH_CANCEL                = 0xAB;  // Cancels a party search
constexpr uint32_t PARTY_SEARCH_REQUEST_JOIN          = 0xAC;  // Requests to join a party search
constexpr uint32_t PARTY_ENTER_FOREIGN_MISSION        = 0xAD;  // Enters a foreign mission/challenge
constexpr uint32_t PARTY_SEARCH_TYPE                  = 0xAE;  // Sets the type of party search
constexpr uint32_t PARTY_READY_STATUS                 = 0xAF;  // Indicates ready status in a party

// ===== Travel =====
constexpr uint32_t GUILDHALL_TRAVEL                   = 0xB0;  // Travels to guild hall
constexpr uint32_t MAP_TRAVEL                         = 0xB1;  // Travels to outpost via worldmap
constexpr uint32_t GUILDHALL_LEAVE                    = 0xB2;  // Leaves guild hall

// ===== Guild =====
constexpr uint32_t BUY_GUILD_CAPE                     = 0x38;  // Header used to buy a cape
constexpr uint32_t SHOW_HIDE                          = 0x57;  // Show or hide helm/cape/costume
constexpr uint32_t GUILD_SET_OFFICER                  = 0xBD;  // Modify guild member role
constexpr uint32_t GUILD_ANNOUCEMENTS                 = 0xBE;  // Modify guild announcement

// ===== Chat =====
constexpr uint32_t SEND_CHAT                          = 0x64;  // Needed for sending messages in chat

// ===== Titles =====
constexpr uint32_t TITLE_DISPLAY                      = 0x58;  // Displays title
constexpr uint32_t TITLE_HIDE                         = 0x59;  // Hides title
constexpr uint32_t HOM_DIALOG                         = 0x60;  // Hall of Monuments dialog
constexpr uint32_t TITLE_UPDATE                       = 0xF5;  // Updates title progress

// ===== Buffs =====
constexpr uint32_t BUFF_DROP                          = 0x29;  // Drops buff / cancel enchantment

// ===== Faction =====
constexpr uint32_t FACTION_DEPOSIT                    = 0x35;  // Donates kurzick/luxon faction to ally

// ===== Misc =====
constexpr uint32_t CINEMATIC_SKIP                     = 0x63;  // Skips the cinematic
constexpr uint32_t OPEN_CHEST                         = 0x53;  // Interacts with chest or signpost

} // namespace GWA3::Packets

// Total: 113 packet headers

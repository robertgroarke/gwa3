#pragma once
// Event handling is split across three layers serving different consumers:
//
//   StoCMgr.h          — Raw server-to-client packet callbacks (pre/post altitude)
//   CallbackRegistry.h — UIMessage callback dispatch (per message-id, altitude-sorted)
//   EventPush.h        — High-level JSON events pushed to the LLM bridge via IPC
//
// StoCMgr replaces the game's StoC handler table with a dispatcher.
// EventPush registers StoC callbacks for key events (MapLoaded, PartyDefeated,
// AgentState, SkillActivate, etc.) and converts them to LLM-friendly JSON.
#include <gwa3/managers/StoCMgr.h>

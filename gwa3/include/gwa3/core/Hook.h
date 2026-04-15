#pragma once
// Hook infrastructure is intentionally distributed — each subsystem owns its hooks:
//
//   RenderHook.h     — Pre-game render detour (character select bootstrap)
//   GameThread.h     — MinHook frame callback (~60fps in-game dispatch)
//   CtoSHook.h       — Inline hook on PacketSend (client-to-server interception)
//   DialogHook.h     — MinHook on UIMessage dispatcher (dialog/frame tracking)
//   TraderHook.h     — Crafter/merchant quote response capture
//   TradePartnerHook.h — Player-to-player trade event tracking
//   TargetLogHook.h  — Target log capture
//
// This file exists as a map. There is no unified Hook manager by design —
// isolated hooks are easier to debug and have independent lifecycles.

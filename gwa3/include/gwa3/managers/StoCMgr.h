#pragma once

// GWA3-053: StoCMgr — Server-to-Client packet callback system.
//
// Enables reactive event monitoring: register callbacks for specific
// incoming packet types (damage events, loot drops, NPC spawns, etc.)
// without polling game state.
//
// Altitude system:
//   altitude <= 0: callback fires BEFORE packet is processed by the game
//   altitude >  0: callback fires AFTER packet is processed

#include <cstdint>
#include <functional>

namespace GWA3::StoC {

// Base packet — all StoC packets start with a uint32_t header
struct PacketBase {
    uint32_t header;
};

// Hook status — callbacks can set blocked=true to prevent further processing
struct HookStatus {
    bool blocked;
};

// Hook entry — opaque handle for callback registration/removal
struct HookEntry {
    void* _internal;
};

// Callback signature: (status, raw_packet) — cast packet to specific type
using PacketCallback = std::function<void(HookStatus*, PacketBase*)>;

bool Initialize();
void Shutdown();

// Register a callback for a specific packet header.
// altitude <= 0: fires before game processing. altitude > 0: fires after.
bool RegisterPacketCallback(HookEntry* entry, uint32_t header,
                            const PacketCallback& callback, int altitude = -0x8000);

// Register a post-processing callback (shorthand for positive altitude)
bool RegisterPostPacketCallback(HookEntry* entry, uint32_t header,
                                const PacketCallback& callback);

// Remove a specific callback by header + entry
void RemoveCallback(uint32_t header, HookEntry* entry);

// Remove all callbacks registered with this entry
void RemoveCallbacks(HookEntry* entry);

// Inject a fake packet into the callback chain (for testing)
bool EmulatePacket(PacketBase* packet);

} // namespace GWA3::StoC

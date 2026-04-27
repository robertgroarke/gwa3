#include <gwa3/managers/FriendListMgr.h>
#include <gwa3/core/Offsets.h>
#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>

namespace GWA3::FriendListMgr {

// Scanned function pointers
using AddFriendFn = void(__cdecl*)(const wchar_t*);
using RemoveFriendFn = void(__cdecl*)(const wchar_t*);
using SetStatusFn = void(__cdecl*)(uint32_t);

static AddFriendFn s_addFriendFn = nullptr;
static RemoveFriendFn s_removeFriendFn = nullptr;
static SetStatusFn s_setStatusFn = nullptr;
static bool s_initialized = false;

bool Initialize() {
    if (s_initialized) return true;

    if (Offsets::AddFriend) s_addFriendFn = reinterpret_cast<AddFriendFn>(Offsets::AddFriend);
    if (Offsets::RemoveFriend) s_removeFriendFn = reinterpret_cast<RemoveFriendFn>(Offsets::RemoveFriend);
    if (Offsets::PlayerStatus) s_setStatusFn = reinterpret_cast<SetStatusFn>(Offsets::PlayerStatus);

    s_initialized = true;
    Log::Info("FriendListMgr: Initialized");
    return true;
}

} // namespace GWA3::FriendListMgr

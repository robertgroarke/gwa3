#pragma once

#include <cstdint>

namespace GWA3::FriendListMgr {

    bool Initialize();

    void AddFriend(const wchar_t* name);
    void RemoveFriend(const wchar_t* name);
    void SetPlayerStatus(uint32_t status);

} // namespace GWA3::FriendListMgr

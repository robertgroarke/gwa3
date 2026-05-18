#include <gwa3/managers/MerchantMgr.h>
static bool WaitForMerchantContext(DWORD timeoutMs) {
    static constexpr uint32_t kMerchantRootHash = 3613855137u;
    DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        if (MerchantMgr::GetMerchantItemCount() > 0) return true;
        if (UIMgr::GetFrameByHash(kMerchantRootHash) != 0) return true;
        if (UIMgr::IsFrameVisible(kMerchantRootHash)) return true;
        Sleep(100);
    }
    return false;
}

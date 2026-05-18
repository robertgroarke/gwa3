#include <gwa3/managers/MerchantMgr.h>
static bool IsMerchantContextVisible(const char* label) {
    const uint32_t merchantItemCount = MerchantMgr::GetMerchantItemCount();
    const uintptr_t merchantFrame = UIMgr::GetFrameByHash(3613855137u);
    FroggyFeatureReport("      Merchant probe after %s: frame=0x%08X items=%u",
              label,
              merchantFrame,
              merchantItemCount);
    return merchantFrame != 0 || merchantItemCount > 0;
}

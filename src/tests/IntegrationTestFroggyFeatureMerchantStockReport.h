#include <gwa3/managers/MerchantMgr.h>
static void ReportMerchantStock() {
    const uint32_t itemCount = MerchantMgr::GetMerchantItemCount();
    FroggyFeatureCheck("Merchant has items", itemCount > 0);
    FroggyFeatureReport("  Merchant has %u items", itemCount);
    for (uint32_t slot = 1; slot <= itemCount; ++slot) {
        if (Item* merchantItem = MerchantMgr::GetMerchantItemByPosition(slot)) {
            FroggyFeatureReport("  Merchant slot %u: itemId=%u model=%u value=%u qty=%u",
                      slot, merchantItem->item_id, merchantItem->model_id,
                      merchantItem->value, merchantItem->quantity);
        }
    }
}

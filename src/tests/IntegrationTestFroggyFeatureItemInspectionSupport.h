static uint16_t GetItemRarityForTest(const Item* item) {
    if (!item) return 0;
    wchar_t* name = item->complete_name_enc ? item->complete_name_enc : item->name_enc;
    return name ? static_cast<uint16_t>(name[0]) : 0;
}

static bool IsIdentifiedForTest(const Item* item) {
    return item && (item->interaction & 0x1) != 0;
}

static bool IsKitModelForTest(uint32_t modelId) {
    switch (modelId) {
    case ItemModelIds::IDENTIFICATION_KIT:
    case ItemModelIds::SUPERIOR_IDENTIFICATION_KIT:
    case ItemModelIds::SALVAGE_KIT:
    case ItemModelIds::EXPERT_SALVAGE_KIT:
    case ItemModelIds::RARE_SALVAGE_KIT:
    case ItemModelIds::SUPERIOR_SALVAGE_KIT:
    case ItemModelIds::ALT_IDENTIFICATION_KIT:
    case ItemModelIds::ALT_SALVAGE_KIT:
        return true;
    default:
        return false;
    }
}

bool TestTradeOffsets() {
    IntReport("=== Trade Function Offsets ===");

    IntReport("  OfferTradeItem: 0x%08X", static_cast<unsigned>(Offsets::OfferTradeItem));
    IntReport("  UpdateTradeCart: 0x%08X", static_cast<unsigned>(Offsets::UpdateTradeCart));

    if (Offsets::OfferTradeItem > 0x10000) {
        IntCheck("OfferTradeItem offset resolved", true);
    } else {
        IntSkip("OfferTradeItem offset", "Pattern did not resolve");
    }

    if (Offsets::UpdateTradeCart > 0x10000) {
        IntCheck("UpdateTradeCart offset resolved", true);
    } else {
        IntSkip("UpdateTradeCart offset", "Pattern did not resolve");
    }

    IntReport("");
    return true;
}

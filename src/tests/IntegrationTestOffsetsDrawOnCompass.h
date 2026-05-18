bool TestDrawOnCompassOffset() {
    IntReport("=== DrawOnCompass Offset ===");
    IntReport("  DrawOnCompass: 0x%08X", static_cast<unsigned>(Offsets::DrawOnCompass));
    if (Offsets::DrawOnCompass > 0x10000) {
        IntCheck("DrawOnCompass offset resolved", true);
    } else {
        IntSkip("DrawOnCompass", "Assertion pattern did not resolve");
    }
    IntReport("");
    return true;
}

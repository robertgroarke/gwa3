// Offset calibration smoke-test section. Included by SmokeTest.cpp inside GWA3::SmokeTest.

struct SmokeRawOffsetMatches {
    uintptr_t raw_base_pointer = 0u;
    uintptr_t raw_my_id = 0u;
    uintptr_t raw_ping = 0u;
    uintptr_t raw_instance_info = 0u;
};

static SmokeRawOffsetMatches RunOffsetCalibrationSmokeSection() {
    SmokeRawOffsetMatches matches;

    Report("--- Offset Byte Dumps (for calibration) ---");
    // These dump raw scan results before post-processing. Since post-processing
    // already ran, re-scan to get the raw values.

    matches.raw_base_pointer = Scanner::Find("\x50\x6A\x0F\x6A\x00\xFF\x35", "xxxxxxx", 0);
    if (matches.raw_base_pointer) {
        Report("BasePointer raw match at 0x%08X (offset 0 from pattern)", matches.raw_base_pointer);
        DumpBytes("BasePointer +0..+16", matches.raw_base_pointer, 0, 16);
        uintptr_t operand = *reinterpret_cast<uint32_t*>(matches.raw_base_pointer + 7);
        Report("  FF 35 operand (BasePointer addr): 0x%08X", operand);
        if (operand > 0x10000) {
            uintptr_t value = *reinterpret_cast<uint32_t*>(operand);
            Report("  *BasePointer = 0x%08X", value);
        }
    }

    matches.raw_my_id = Scanner::Find("\x83\xEC\x08\x56\x8B\xF1\x3B\x15", "xxxxxxxx", 0);
    if (matches.raw_my_id) {
        Report("MyID raw match at 0x%08X", matches.raw_my_id);
        DumpBytes("MyID +0..+16", matches.raw_my_id, 0, 16);
        uintptr_t operand = *reinterpret_cast<uint32_t*>(matches.raw_my_id + 8);
        Report("  3B 15 operand (MyID addr): 0x%08X", operand);
        if (operand > 0x10000) {
            uint32_t value = *reinterpret_cast<uint32_t*>(operand);
            Report("  *MyID = %u (0x%08X)", value, value);
        }
    }

    matches.raw_ping = Scanner::Find("\x56\x8B\x75\x08\x89\x16\x5E", "xxxxxxx", 0);
    if (matches.raw_ping) {
        Report("Ping raw match at 0x%08X", matches.raw_ping);
        DumpBytes("Ping -4..+12", matches.raw_ping, 4, 12);
        uintptr_t withOffset = matches.raw_ping - 3;
        Report("  Ping with offset -3: 0x%08X", withOffset);
        DumpBytes("Ping@offset", withOffset, 0, 8);
        uintptr_t operand = *reinterpret_cast<uint32_t*>(withOffset);
        Report("  Deref at offset: 0x%08X", operand);
    }

    matches.raw_instance_info =
        Scanner::Find("\x6A\x2C\x50\xE8\x00\x00\x00\x00\x83\xC4\x08\xC7", "xxxx????xxxx", 0);
    if (matches.raw_instance_info) {
        Report("InstanceInfo raw match at 0x%08X", matches.raw_instance_info);
        DumpBytes("InstanceInfo +12..+20", matches.raw_instance_info + 12, 0, 12);
        uintptr_t withOffset = matches.raw_instance_info + 0xE;
        uintptr_t operand = *reinterpret_cast<uint32_t*>(withOffset);
        Report("  InstanceInfo operand at +0xE: 0x%08X", operand);
        if (operand > 0x10000) {
            uint32_t mapId = *reinterpret_cast<uint32_t*>(operand);
            Report("  *InstanceInfo (MapID?) = %u", mapId);
        }
    }
    Report("");

    return matches;
}

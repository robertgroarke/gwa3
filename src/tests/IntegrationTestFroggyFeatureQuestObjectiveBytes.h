static bool TryCaptureQuestObjectiveBytes(const Quest* quest, uint8_t (&bytes)[8]) {
    const auto* objective = reinterpret_cast<const uint8_t*>(quest ? quest->objectives : nullptr);
    bool captured = false;
    __try {
        if (objective) {
            for (size_t i = 0; i < 8; ++i) {
                bytes[i] = objective[i];
            }
            captured = true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        captured = false;
    }
    return captured;
}

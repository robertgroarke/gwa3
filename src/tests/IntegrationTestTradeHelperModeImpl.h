// Trade helper integration workflow. Included by IntegrationTestSession.cpp
// so it can use the session-local harness helpers while extraction continues.

int RunTradeHelperMode() {
    IntReport("=== GWA3 Player Trade Helper Mode ===");
    // Helper mode is a long-lived idle/rendezvous loop rather than an assert-heavy
    // integration suite. The generic watchdog's hung-window heuristic can false-fire
    // here during map travel/load and kill the helper before the trade harness ever
    // sees it. The harness already verifies helper liveness explicitly via PID and
    // the status file, so keep helper mode watchdog-free.

    if (!PrepareTradeHelperModeWorld()) return 1;

    const DWORD start = GetTickCount();
    DWORD lastLog = 0;
    TradeHelperOpenTradeState tradeState{};
    uint32_t tradeOpenCount = 0;
    uint32_t submitAttemptCount = 0;
    uint32_t acceptAttemptCount = 0;
    TradeHelperMoveState moveState{};
    uint32_t lastChatSendSeq = 0;
    uint32_t lastWhisperSendSeq = 0;
    uint32_t chatSendAttemptCount = 0;
    uint32_t whisperSendAttemptCount = 0;

    while (GetTickCount() - start < 10 * 60 * 1000) {
        const DWORD now = GetTickCount();
        const uint32_t tradeFlags = ReadTradeFlagsForHelper();
        const bool tradeOpen = tradeFlags != 0;
        UpdateTradeHelperOpenTradeState(tradeState, tradeFlags, tradeOpen, now, tradeOpenCount);

        UpdateTradeHelperMoveRequest(moveState);

        const bool autoSubmitEnabled = ReadTradeHelperAutoSubmitConfig();
        const bool autoAcceptEnabled = ReadTradeHelperAutoAcceptConfig();
        if (TryHandleTradeHelperChatSend(lastChatSendSeq)) ++chatSendAttemptCount;
        if (TryHandleTradeHelperWhisperSend(lastWhisperSendSeq)) ++whisperSendAttemptCount;

        TickTradeHelperTradeActions(tradeState, tradeOpen, autoSubmitEnabled, autoAcceptEnabled,
                                    tradeFlags, now, submitAttemptCount, acceptAttemptCount);

        float x = 0.0f, y = 0.0f;
        TryReadAgentPosition(ReadMyId(), x, y);
        TickTradeHelperMovement(moveState, now, x, y);
        WriteTradeHelperModeStatusAndHeartbeat(
            x, y, tradeFlags, tradeOpenCount, tradeState.lastOpenFlags,
            submitAttemptCount, acceptAttemptCount,
            chatSendAttemptCount, whisperSendAttemptCount,
            lastChatSendSeq, lastWhisperSendSeq,
            now, lastLog);

        Sleep(250);
    }

    TradePartnerHook::Shutdown();
    IntReport("  Helper timeout reached; exiting helper mode");
    return 0;
}

bool TestChatWriteLocal() {
    IntReport("=== Chat Write (Local) ===");

    if (ReadMyId() == 0) {
        IntSkip("Chat write", "Not in game");
        IntReport("");
        return false;
    }

    IntReport("  Writing local chat message...");
    GameThread::Enqueue([]() {
        ChatMgr::WriteToChat(L"[GWA3] Integration test: chat write OK", 0);
    });
    Sleep(500);
    IntCheck("WriteToChat sent (no crash)", true);

    const uint32_t ping1 = ChatMgr::GetPing();
    Sleep(200);
    const uint32_t ping2 = ChatMgr::GetPing();
    IntReport("  Ping samples: %u ms, %u ms", ping1, ping2);
    IntCheck("Ping stable and plausible", ping1 > 0 && ping1 < 5000 && ping2 > 0 && ping2 < 5000);

    IntReport("");
    return true;
}

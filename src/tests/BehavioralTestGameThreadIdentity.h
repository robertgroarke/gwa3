static void RunBehavioralGameThreadIdentityTest() {
    CmdReport("--- Test 2: IsOnGameThread ---");
    std::atomic<bool> wasOnGameThread{false};
    GameThread::Enqueue([&wasOnGameThread]() {
        wasOnGameThread = GameThread::IsOnGameThread();
    });
    Sleep(1000);
    CmdCheck("IsOnGameThread() true inside Enqueue", wasOnGameThread.load());
    CmdCheck("IsOnGameThread() false from init thread", !GameThread::IsOnGameThread());
    CmdReport("");
}

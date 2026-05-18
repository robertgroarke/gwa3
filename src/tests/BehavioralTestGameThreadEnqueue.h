static void RunBehavioralGameThreadEnqueueTest() {
    CmdReport("--- Test 1: Game Thread Enqueue ---");
    std::atomic<bool> flag{false};
    GameThread::Enqueue([&flag]() {
        flag = true;
    });
    Sleep(2000); // Wait for next frame tick
    CmdCheck("GameThread::Enqueue fires callback", flag.load());
    CmdReport("");
}

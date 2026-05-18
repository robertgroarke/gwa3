static void ReportMerchantRuntimeContext(const char* label) {
    FroggyFeatureReport("  %s: GameThread=%d onGameThread=%d RenderHook=%d hb=%u TraderHook=%d TargetLogHook=%d targetCalls=%u targetStores=%u CtoSHook=%d ctoSHb=%u",
              label,
              GameThread::IsInitialized() ? 1 : 0,
              GameThread::IsOnGameThread() ? 1 : 0,
              RenderHook::IsInitialized() ? 1 : 0,
              RenderHook::GetHeartbeat(),
              TraderHook::IsInitialized() ? 1 : 0,
              TargetLogHook::IsInitialized() ? 1 : 0,
              TargetLogHook::GetCallCount(),
              TargetLogHook::GetStoreCount(),
              CtoSHook::IsInitialized() ? 1 : 0,
              CtoSHook::GetHeartbeat());
}

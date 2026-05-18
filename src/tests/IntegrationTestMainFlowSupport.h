// Branch flows for the default integration suite. Included by IntegrationTestMainRunner.cpp.

static void SkipMainIntegrationSuiteForLoginFailure() {
    IntSkip("Hero Setup (029)", "Login failed");
    IntSkip("Movement (030)", "Login failed");
    IntSkip("Targeting (030)", "Login failed");
    IntSkip("Skill activation (034)", "Login failed");
    IntSkip("NPC + Dialog (032)", "Login failed");
    IntSkip("Outpost Travel (033)", "Login failed");
    IntSkip("Explorable Entry (035)", "Login failed");
    IntSkip("Loot (031)", "Login failed");
    IntSkip("Merchant (032)", "Login failed");
    IntSkip("Advanced tests (036-049)", "Login failed");
}

static void RunExplorableStartIntegrationFlow(bool sessionHydrated) {
    IntReport("  Branching integration flow: explorable-start session");
    IntSkip("Outpost Travel (033)", "Bootstrap landed in explorable instance");
    IntSkip("Explorable Entry (035)", "Already in explorable instance");

    if (!sessionHydrated) {
        IntSkip("Movement (030)", "Explorable session never reached stable runtime readiness");
        IntSkip("Targeting (030)", "Explorable session never reached stable runtime readiness");
        IntSkip("Skill activation (034)", "Explorable session never reached stable runtime readiness");
        IntSkip("Loot pickup (031)", "Explorable session never reached stable runtime readiness");
        IntSkip("Hero Setup (029)", "Deferred hero setup because explorable session never stabilized");
        IntSkip("Advanced tests (036-049)", "Explorable session never stabilized");
        return;
    }

    // Keep the explorable-start branch focused on world-action coverage first;
    // hero setup appears to destabilize this session class.
    TestMovement();
    TestTargeting();
    TestSkillActivation();
    TestLootPickup();

    IntReport("  Running deferred hero setup after explorable actions...");
    TestHeroSetup();

    // Advanced introspection tests (read-only, safe in any map type)
    IntReport("  Running advanced introspection tests...");
    TestPlayerData();
    TestCameraIntrospection();
    TestClientInfo();
    TestInventoryIntrospection();
    TestAgentArrayEnumeration();
    TestUIFrameValidation();
    TestAreaInfoValidation();
    TestSkillbarDataValidation();
    TestPartyState();
    TestTargetLogHook();
    TestGuildData();
    TestMapStateQueries();
    TestPingStability();
    TestWeaponSetValidation();
    TestAgentDistanceCrossCheck();
    TestCameraControls();
    TestChatWriteLocal();

    TestReturnToOutpost();
}

static void RunOutpostStartIntegrationFlow() {
    IntReport("  Branching integration flow: outpost-start session");

    // Phase 2: hero setup
    TestHeroSetup();

    // Phase 3: movement
    TestMovement();

    // Phase 4: Targeting
    TestTargeting();

    TestHardModeToggle();

    // Phase 5: explorable bootstrap
    IntSkip("Outpost Travel (033)", "Session reserved for explorable skill coverage");
    const bool inExplorable = TestExplorableEntry();

    if (!inExplorable) {
        IntSkip("Skill activation (034)", "Explorable entry failed");
        IntSkip("Loot pickup (031)", "Explorable entry failed");
        IntSkip("Return to outpost (047)", "Explorable entry failed");
        return;
    }

    // Wait for explorable to fully load (agents, navmesh, etc.)
    IntReport("  Waiting for explorable runtime to stabilize...");
    WaitFor("explorable map loaded + agents available", 15000, []() {
        if (!MapMgr::GetIsMapLoaded()) return false;
        if (ReadMyId() == 0) return false;
        AgentLiving* me = AgentMgr::GetMyAgent();
        if (!me || me->hp <= 0.0f) return false;
        return AgentMgr::GetMaxAgents() > 10;
    });

    // Keep explorable coverage stable: post-load hero flagging is disabled until
    // Sparkfly hero-command timing is understood.
    IntSkip("Hero Flagging (043)", "Disabled in explorable pending Sparkfly hero-command stabilization");

    TestSkillActivation();
    TestLootPickup();

    TestReturnToOutpost();
}

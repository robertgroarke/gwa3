static void RunBehavioralMovementTest() {
    CmdReport("--- Test 3: Movement ---");
    auto* me = AgentMgr::GetMyAgent();
    if (me) {
        float startX = me->x;
        float startY = me->y;
        CmdReport("Start position: (%.1f, %.1f)", startX, startY);

        CtoS::MoveToCoord(startX + 200.0f, startY);
        Sleep(3000);

        me = AgentMgr::GetMyAgent(); // re-read
        if (me) {
            float endX = me->x;
            float endY = me->y;
            float dist = sqrtf((endX - startX) * (endX - startX) + (endY - startY) * (endY - startY));
            CmdReport("End position: (%.1f, %.1f), moved %.1f units", endX, endY, dist);
            CmdCheck("Character moved > 50 units", dist > 50.0f);
        } else {
            CmdCheck("Agent readable after move", false);
        }

        CtoS::MoveToCoord(startX, startY);
        Sleep(2000);
    } else {
        CmdReport("[SKIP] No player agent - cannot test movement");
    }
    CmdReport("");
}

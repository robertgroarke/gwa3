# Legacy / Deprecated Code Cleanup Assessment

## Items Removed

### 1. Dead legacy vectors in GameThread.cpp (lines 59-61)
- `s_queue` and `s_postQueue` (std::vector<Callback>) declared with comment "Legacy -- kept for API compatibility"
- The old Dispatch/DispatchPost functions that used them were already removed
- These vectors are never referenced anywhere in the codebase
- Also removed stale comment on line 71: "Old Dispatch/DispatchPost removed..."

### 2. `#if 0` block in FroggyHM.cpp (lines 1070-1122)
- Old CanCast implementation replaced by ExplainCanCastFailure + CanCast wrapper
- The function body now delegates to `ExplainCanCastFailure(skill) == nullptr`
- The `#if 0` block contains the exact same logic that ExplainCanCastFailure implements
- 53 lines of dead commented-out code

### 3. `#if 0` stub in IntegrationTestSession.cpp (lines 3965-3970)
- `RunFroggySparkflyRouteTest()` stub that says "not implemented in this file"
- Never called, compiled out

### 4. Deprecated GetAgentArray() function
- Returns nullptr unconditionally with comment "Legacy API -- returns null"
- Zero callers in the entire codebase
- Removed from both AgentMgr.h and AgentMgr.cpp

### 5. Unused ROLE_ANY_CLASSIFIED constant in FroggyHM.cpp
- Defined as 0xFFFFFFFFu but never referenced anywhere

### 6. Misleading "GameThread failed" warning in dllmain.cpp (lines 331-334)
- `gameThreadOk` initialized to false, immediately checked, always prints warning
- GameThread::Initialize() is called later at line 351
- Removed the spurious always-firing warning block

### 7. Commented-out CtoSHook::Initialize() in dllmain.cpp (line 356)
- `// GWA3::CtoSHook::Initialize();` with comment about being disabled
- The comment above it explains why; removed the dead line (the explanatory comment stays)

### 8. Bisection flags in IntegrationTest.cpp (lines 44-49)
- Six `kBisect*` constexpr bool flags all set to false
- All associated `if (kBisect*) { ... goto workflow_done; }` blocks are dead code
- These are developer crash-bisection toggles with zero overhead (constexpr false)
- Removed definitions and all six conditional blocks

### 9. Misleading "legacy" comments on actively-used packet headers
- DIALOG_SEND (0x3B) and INTERACT_NPC (0x39) labeled "legacy" but are the primary working headers
- Updated comments to accurately describe them as alternative/distinct opcodes
- Also updated "Combined masks for legacy compatibility" comment in FroggyHM.cpp

## Items Retained (Not Legacy)

### Legitimate Runtime Fallbacks
- **UIMgr RenderHook shellcode fallback**: Used when GameThread is unavailable during startup
- **QuestMgr raw packet fallback**: Used when native function pointer isn't resolved
- **AgentMgr InteractNPC raw packet fallback**: Same pattern -- native path preferred, packet as backup
- **TradeMgr UIMessage fallback**: Direct function call when UIMessage not available
- **ClickPlayButtonMouseFallback in dllmain.cpp**: Active bootstrap fallback for char select

### Debug Infrastructure
- **CRASH_TEST compile-time flag**: Build-time configurable (CMakeLists.txt), not dead code
- **kDisableTradeCartHookForDebug**: Player trade is known broken; flag is intentional
- **MerchantDialogVariant::LegacyId/LegacyPtr**: Active test variants selectable via flag files

### Deprecated-by-Design API
- **HandleKickAllHeroes**: Intentionally returns error to force callers to use individual kicks
- Has tests validating the deprecation error; removing would break test_c_actions and test_e_orchestrated

### External Code
- All `build_*/_deps/json-src/` matches are third-party nlohmann/json -- not our code

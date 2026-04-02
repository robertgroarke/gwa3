---
name: GW injection constraints
description: One injection per GW session, must handle char select + in-game from single DLL
type: feedback
---

Only ONE DLL injection per fresh GW client — cannot inject twice without crash.
AutoIt injection and gwa3.dll injection are mutually exclusive.

**Why:** GW's module loader / injection detection crashes on re-injection.

**How to apply:**
- gwa3.dll must handle the ENTIRE lifecycle: char select Play click → in-game hooks
- Inject at char select with BEASTRIT already selected (via -character flag from GW Launcher)
- Click Play via SendFrameUIMsg from within gwa3.dll (proven approach from AutoIt FrameUI)
- After map loads (MapID > 0), THEN enable GameThread hooks and game commands
- GWCA/GWToolbox pattern: inject early but defer hooks until character is in-game

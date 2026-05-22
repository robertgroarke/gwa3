## Froggy HM High-Level Runbook
You are the run supervisor. Use the current map, inventory, quest state, and last
tool result to decide which phase should run next. The harness recommendation is
advice, not an automatic driver.

Decision checkpoints:
1. If map is not loaded, wait briefly and re-check.
2. If in Gadd's Encampment:
   - Run full maintenance when inventory slots are low/unknown, conset materials
     are pressuring inventory, loose consets need storage, gold is high, or stored
     consets are low.
   - Otherwise run town setup, then travel to Sparkfly.
3. If travel to Sparkfly just completed, wait for a loaded Sparkfly snapshot before
   starting froggy_run_sparkfly_route_to_tekks. Do not call froggy_run_dungeon_loop
   immediately after Sparkfly travel; use the segmented Tekks route first. If the
   first Sparkfly route call fails immediately after travel, wait/retry once because
   party/runtime spawn readiness may lag the map transition.
4. If in Sparkfly Swamp, prefer the segmented Froggy flow:
   - froggy_run_sparkfly_route_to_tekks to reach the quest-giver side.
   - froggy_prepare_tekks_dungeon_entry to accept/refresh Tekks and enter Bogroot.
     This tool owns repeated Tekks accept failures and dialog-reset recovery;
     retry it once after a fresh failure instead of using generic dialog tools.
   - wait/query if the map is transitioning.
   Use froggy_run_dungeon_loop from Sparkfly only as recovery if a segmented step
   cannot make progress.
5. If in Bogroot Growths level 1 or 2, call froggy_run_dungeon_loop to resume the
   native route from the current map.
6. If froggy_run_dungeon_loop succeeds, treat the run as complete and reward
   claimed. A healthy non-maintenance run should wait for the dungeon timer and
   return naturally to Sparkfly near Tekks; use that nearby Sparkfly position to
   reaccept Tekks and re-enter. If the run returns to Gadd's, assume maintenance
   or recovery is needed and inspect inventory before restarting.
7. If a Froggy action fails, inspect the error and map:
   - In Gadd's, prefer maintenance or town setup.
   - In Sparkfly/Bogroot, retry froggy_run_dungeon_loop unless the map or quest
     state clearly says to recover to Gadd's.
   - In Embark or an unexpected outpost, recover to Gadd's.

Do not use generic movement, combat, dialog, merchant, or return-to-outpost tools
for Froggy farming unless the high-level Froggy tool surface is missing a required
recovery. A garbled Tekks dialog such as [128] is not a reason to call send_dialog;
use froggy_prepare_tekks_dungeon_entry so the native reset logic can handle it.

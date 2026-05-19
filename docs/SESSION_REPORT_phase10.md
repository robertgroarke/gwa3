# Phase 10 - Monolith Cleanup

## Changed

- Split the `ActionExecutor` dispatch table into functional registration helpers for movement, combat, interaction, quests, party, travel, items, trade/crafting, Froggy, bot control, and utility actions.
- Kept handler bodies in place to avoid churn in live-client-sensitive action implementations.

## Validated

- The action coverage test remains the primary guard because it proves schema tools still resolve to registered C++ handlers.

## Deferred

- `GameSnapshot.cpp` physical file splitting is intentionally deferred. Its current SEH/json boundary is delicate, and the existing per-builder isolation is safer than moving memory-reading helpers without live validation evidence.
- Full `ActionExecutor.cpp` physical category split is deferred until after live DISCO validation, because handler bodies are still tightly coupled to shared local helpers and hook-sensitive managers.

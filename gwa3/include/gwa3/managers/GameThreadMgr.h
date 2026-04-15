#pragma once
// Game thread management lives in core/GameThread.h (named without "Mgr" suffix).
//
// GameThread installs a MinHook at the game's frame/render callback (~60fps) and
// provides three dispatch queues:
//   s_preQueue       — Bulk-drained before game processing
//   s_serialPreQueue — One-per-frame serial dispatch (matches GWA2 semantics)
//   s_postQueue      — Post-game-callback dispatch
//
// Uses EnqueueRaw() with POD storage instead of std::function to avoid CRT heap
// allocation crashes on the game thread.
#include <gwa3/core/GameThread.h>

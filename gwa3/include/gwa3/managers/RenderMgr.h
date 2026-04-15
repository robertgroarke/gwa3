#pragma once
// Render/frame management uses two systems:
//
//   RenderHook.h (core/) — Pre-game render detour at mid-function seam.
//     Used for character select bootstrap (button clicks before map load).
//     Naked detour assembly with 256-entry command queue.
//     Self-disables after map load (trampoline crashes after ~2300 frames).
//
//   GameThread.h (core/) — MinHook frame callback (~60fps, stable in-game).
//     Modern replacement for render-time dispatch. Persistent per-frame
//     callback registration with altitude sorting.
//
// RenderHook handles pre-game; GameThread handles in-game. Both coexist.
#include <gwa3/core/RenderHook.h>
#include <gwa3/core/GameThread.h>

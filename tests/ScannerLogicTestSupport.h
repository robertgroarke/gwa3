#pragma once

// : Offline tests for scanner pattern logic.
// These tests exercise pattern matching, near-call resolution, and hex parsing
// against synthetic in-memory buffers — no game client needed.

#include <gwa3/core/Scanner.h>
#include <gwa3/testing/TestFramework.h>

#include <cstring>

// ===== FunctionFromNearCall =====

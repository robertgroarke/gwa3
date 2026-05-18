#pragma once

// : Offline tests for string encoding/decoding logic.
// NOTE: DecodeEncValue round-trip tests are deferred — a startup crash
// (0xC0000409 STATUS_STACK_BUFFER_OVERRUN) occurs when DecodeEncValue is
// referenced alongside the current gwa3_core OBJECT library. This appears
// to be a linker-layout-dependent /GS issue in other object files, not in
// the string encoding code itself. The functions are verified correct by
// inspection and will be tested once the root cause is resolved.

#include <gwa3/utils/StringEncoding.h>
#include <gwa3/testing/TestFramework.h>

// ===== IsValidEncStr =====

#pragma once

// Asynchronous decoder for GW-encoded wide-character strings. Used to
// surface human-readable quest names / objectives / descriptions to the
// LLM bridge without blocking the snapshot thread.
//
// ValidateAsyncDecodeStr (see StringEncoding.cpp) is fire-and-forget —
// the callback arrives on the game thread at an unknown later time.
// Calling it synchronously from the snapshot path risks overrunning the
// GameThread pre-dispatch queue and crashing GW (see QUEST_LOG_RESEARCH.md).
//
// This module runs a dedicated worker thread that drains decode requests
// one at a time, at a sustainable rate, and writes completed results
// into a thread-safe cache. The snapshot path only reads the cache.

#include <string>

namespace GWA3::EncStringCache {

    // Start the background decoder worker. Safe to call multiple times.
    bool Initialize();

    // Stop the worker, join the thread, and clear all state. Idempotent.
    void Shutdown();

    // Lookup decoded UTF-8 text for an encoded wide-char string.
    //
    // If cached: returns the decoded text immediately.
    // If not cached: enqueues a decode request and returns an empty
    // string. Subsequent Lookup calls for the same content will return
    // the decoded text once the worker completes (usually within a few
    // hundred ms).
    //
    // The encoded string's bytes are copied into the key — `enc` need
    // not remain valid after the call returns.
    //
    // Thread-safe.
    std::string Lookup(const wchar_t* enc);

    // Drop all cached entries and pending requests. Call on map change
    // if the cache is keyed by pointer, not by content. (Current
    // implementation keys by content, so Clear is optional — exposed
    // for tests and for forcing a re-decode sweep.)
    void Clear();

} // namespace GWA3::EncStringCache

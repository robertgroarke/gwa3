#pragma once

// Asynchronous decoder for GW-encoded wide-character strings. Used to
// surface human-readable quest names / objectives / descriptions to the
// LLM bridge without blocking the snapshot thread.
//
// ValidateAsyncDecodeStr (see StringEncoding.cpp) is fire-and-forget:
// the callback arrives on the game thread at an unknown later time.
// Calling it synchronously from the snapshot path risks overrunning the
// GameThread pre-dispatch queue and crashing GW.
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

    // Read-only cache lookup. Returns decoded UTF-8 text when cached,
    // or an empty string when not. Never enqueues a decode.
    //
    // Safe to call on every snapshot; the snapshot path uses this so
    // decoded text appears automatically once the cache is primed, but
    // without ever triggering new decode work itself.
    //
    // The encoded string's bytes are copied into the key — `enc` need
    // not remain valid after the call returns. Thread-safe.
    std::string Lookup(const wchar_t* enc);

    // Request that `enc` be decoded in the background if it is not
    // already cached or pending. Returns immediately.
    //
    // Decode volume is deliberately bounded here: callers should only
    // Prime strings in response to an explicit signal (e.g. an LLM
    // `request_quest_info` action), not on every snapshot — driving
    // ValidateAsyncDecodeStr at snapshot rate destabilises GW.
    void Prime(const wchar_t* enc);

    // Direct-insert a decoded UTF-8 value for `enc`. Used by the passive
    // MinHook detour on ValidateAsyncDecodeStr to populate the cache
    // whenever GW's own UI pipeline decodes a string. Safe to call from
    // any thread.
    void InsertDecoded(const wchar_t* enc, std::string decoded);

    // Drop all cached entries and pending requests. Call on map change
    // if the cache is keyed by pointer, not by content. (Current
    // implementation keys by content, so Clear is optional — exposed
    // for callers that need to force a re-decode sweep.)
    void Clear();

} // namespace GWA3::EncStringCache

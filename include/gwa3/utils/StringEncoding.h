#pragma once

// String Encoding/Decoding for Guild Wars encoded text.
//
// GW uses variable-length encoded wchar_t* strings for quest names, item names,
// NPC names, skill descriptions, etc. These must be decoded via a game function
// (ValidateAsyncDecodeStr) which calls back asynchronously with the decoded text.
//
// Encoding format:
//   Each word (uint16_t) encodes 7 bits of payload (bits 8-14).
//   Bit 15 (0x8000) = continuation flag (more words follow).
//   Words < 0x100 are literal characters or terminators.

#include <cstdint>

namespace GWA3::StringEncoding {

    // Initialize: resolve ValidateAsyncDecodeStr from Offsets.
    bool Initialize();

    // Decode an encoded game string to readable text (blocking).
    // Enqueues on the game thread, waits up to timeoutMs for completion.
    // Returns number of characters written (excluding null), or 0 on failure/timeout.
    uint32_t DecodeStr(const wchar_t* encStr, wchar_t* outBuf, uint32_t outBufSize,
                       uint32_t timeoutMs = 1000);

    // Validate that a string is in encoded format (starts with word >= 0x100).
    bool IsValidEncStr(const wchar_t* str);

    // Decode the first encoded integer value from an encoded string.
    // GW encodes integers as variable-length sequences of words:
    //   payload = (word & 0x7F00) >> 8, continuation = (word & 0x8000)
    // Returns the decoded value, or 0 if invalid.
    uint32_t DecodeEncValue(const wchar_t* encStr);

    // Encode an integer value into GW's encoded string format.
    // Writes encoded words to outBuf (max outCount words including null terminator).
    // Returns number of words written (excluding null), or 0 on failure.
    uint32_t UInt32ToEncStr(uint32_t value, wchar_t* outBuf, uint32_t outCount);

} // namespace GWA3::StringEncoding

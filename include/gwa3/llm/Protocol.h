#pragma once

#include <cstdint>

namespace GWA3::LLM {

inline constexpr uint32_t IPC_PROTOCOL_VERSION = 1;
inline constexpr const char* TOOL_SCHEMA_VERSION = "gwa3-tools-v1";

template <typename Json>
inline void StampProtocol(Json& message) {
    message["protocol_version"] = IPC_PROTOCOL_VERSION;
    message["v"] = IPC_PROTOCOL_VERSION;
}

template <typename Json>
inline int ReadProtocolVersion(const Json& message) {
    if (message.contains("protocol_version")) {
        return message.value("protocol_version", -1);
    }
    return message.value("v", -1);
}

} // namespace GWA3::LLM

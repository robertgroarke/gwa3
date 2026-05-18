struct HelperArrayView {
    T* buffer;
    uint32_t capacity;
    uint32_t size;
    uint32_t param;
};

struct HelperTradeContextView {
    struct Item {
        uint32_t item_id;
        uint32_t quantity;
    };
    struct Trader {
        uint32_t gold;
        HelperArrayView<Item> items;
    };
    uint32_t flags;
    uint32_t h0004[3];
    Trader player;
    Trader partner;
};

static std::string JsonEscapeUtf8(const char* text) {
    std::string out;
    if (!text) return out;
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
        switch (*p) {
        case '\"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (*p < 0x20) {
                char buf[8] = {};
                sprintf_s(buf, "\\u%04X", static_cast<unsigned>(*p));
                out += buf;
            } else {
                out.push_back(static_cast<char>(*p));
            }
            break;
        }
    }
    return out;
}

IdentifySalvageIsolationStage GetIdentifySalvageIsolationStage() {
    if (CheckLocalFlagFile("gwa3_test_identsalvage_stage_identify_only.flag")) {
        return IdentifySalvageIsolationStage::IdentifyOnly;
    }
    if (CheckLocalFlagFile("gwa3_test_identsalvage_stage_salvage_open_only.flag")) {
        return IdentifySalvageIsolationStage::SalvageOpenOnly;
    }
    if (CheckLocalFlagFile("gwa3_test_identsalvage_stage_single_salvage.flag")) {
        return IdentifySalvageIsolationStage::SingleSalvage;
    }
    if (CheckLocalFlagFile("gwa3_test_identsalvage_stage_native_salvage.flag")) {
        return IdentifySalvageIsolationStage::NativeSalvage;
    }
    if (CheckLocalFlagFile("gwa3_test_identsalvage_stage_native_salvage_enter.flag")) {
        return IdentifySalvageIsolationStage::NativeSalvageEnter;
    }
    if (CheckLocalFlagFile("gwa3_test_identsalvage_stage_legacy_botshub_start_only.flag")) {
        return IdentifySalvageIsolationStage::LegacyBotshubStartOnly;
    }
    if (CheckLocalFlagFile("gwa3_test_identsalvage_stage_legacy_botshub_salvage.flag")) {
        return IdentifySalvageIsolationStage::LegacyBotshubSalvage;
    }
    if (CheckLocalFlagFile("gwa3_test_identsalvage_stage_legacy_botshub_salvage_enter.flag")) {
        return IdentifySalvageIsolationStage::LegacyBotshubSalvageEnter;
    }
    if (CheckLocalFlagFile("gwa3_test_identsalvage_stage_legacy_botshub_salvage_done.flag")) {
        return IdentifySalvageIsolationStage::LegacyBotshubSalvageDone;
    }
    if (CheckLocalFlagFile("gwa3_test_identsalvage_stage_legacy_botshub_salvage_cancel.flag")) {
        return IdentifySalvageIsolationStage::LegacyBotshubSalvageCancel;
    }
    if (CheckLocalFlagFile("gwa3_test_identsalvage_stage_legacy_botshub_tracked_chain.flag")) {
        return IdentifySalvageIsolationStage::LegacyBotshubTrackedChain;
    }
    return IdentifySalvageIsolationStage::Full;
}

const char* DescribeIdentifySalvageIsolationStage(IdentifySalvageIsolationStage stage) {
    switch (stage) {
    case IdentifySalvageIsolationStage::Full: return "full";
    case IdentifySalvageIsolationStage::IdentifyOnly: return "identify-only";
    case IdentifySalvageIsolationStage::SalvageOpenOnly: return "salvage-open-only";
    case IdentifySalvageIsolationStage::SingleSalvage: return "single-salvage";
    case IdentifySalvageIsolationStage::NativeSalvage: return "native-salvage";
    case IdentifySalvageIsolationStage::NativeSalvageEnter: return "native-salvage-enter";
    case IdentifySalvageIsolationStage::LegacyBotshubStartOnly: return "legacy-botshub-start-only";
    case IdentifySalvageIsolationStage::LegacyBotshubSalvage: return "legacy-botshub-salvage";
    case IdentifySalvageIsolationStage::LegacyBotshubSalvageEnter: return "legacy-botshub-salvage-enter";
    case IdentifySalvageIsolationStage::LegacyBotshubSalvageDone: return "legacy-botshub-salvage-done";
    case IdentifySalvageIsolationStage::LegacyBotshubSalvageCancel: return "legacy-botshub-salvage-cancel";
    case IdentifySalvageIsolationStage::LegacyBotshubTrackedChain: return "legacy-botshub-tracked-chain";
    default: return "unknown";
    }
}

static std::string WideToUtf8String(const wchar_t* text) {
    if (!text || !text[0]) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 1) return {};
    std::string out(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), needed, nullptr, nullptr);
    out.resize(static_cast<size_t>(needed - 1));
    return out;
}

static std::string BuildRecentHelperChatJson(uint32_t maxEntries = 8u) {
    const uint32_t total = ChatLogMgr::GetMessageCount();
    if (total == 0) return "[]";

    const ChatLogMgr::ChatEntry* entries[100] = {};
    const uint32_t count = ChatLogMgr::GetMessagesSince(0, entries, _countof(entries));
    if (count == 0) return "[]";

    const uint32_t start = count > maxEntries ? (count - maxEntries) : 0u;
    std::string out = "[";
    for (uint32_t i = start; i < count; ++i) {
        const auto* entry = entries[i];
        if (!entry) continue;
        if (out.size() > 1) out += ",";
        std::string senderUtf8 = JsonEscapeUtf8(WideToUtf8String(entry->sender).c_str());
        std::string messageUtf8 = JsonEscapeUtf8(WideToUtf8String(entry->message).c_str());
        out += "{\"channel\":\"";
        out += JsonEscapeUtf8(entry->channel_name);
        out += "\",\"sender\":\"";
        out += senderUtf8;
        out += "\",\"message\":\"";
        out += messageUtf8;
        out += "\",\"timestamp\":";
        out += std::to_string(entry->timestamp_ms);
        out += ",\"sender_agent_id\":";
        out += std::to_string(entry->sender_agent_id);
        out += "}";
    }
    out += "]";
    return out;
}

void WriteTradeHelperStatus(uint32_t mapId, uint32_t region, uint32_t district, uint32_t myId, float x, float y,
                            uint32_t tradeFlags, uint32_t tradeOpenCount, uint32_t lastOpenFlags,
                            uint32_t submitAttemptCount, uint32_t acceptAttemptCount,
                            uint32_t chatSendAttemptCount, uint32_t whisperSendAttemptCount,
                            uint32_t lastChatSendSeq, uint32_t lastWhisperSendSeq,
                            uint32_t inventoryFreeSlotsTotal,
                            uint32_t helperStackableOfferModelId, uint32_t helperStackableOfferQuantity,
                            uint32_t helperStackableOfferTotalQuantity,
                            uint32_t helperSafeSingletonOfferModelId,
                            uint32_t helperSafeSingletonOfferTotalQuantity,
                            uint32_t playerGold, uint32_t partnerGold,
                            uint32_t playerItemCount, uint32_t partnerItemCount,
                            uint32_t ctoSPacketTotal, uint32_t ctoSTradeSubmitCount,
                            uint32_t ctoSTradeAcceptCount, uint32_t ctoSTradeCancelCount,
                            uint32_t ctoSTradeAddItemCount,
                            uint32_t tradePartnerHookHits, uint32_t tradePartnerLastEax,
                            uint32_t tradePartnerLastEcx, uint32_t tradePartnerLastEdx,
                            uint32_t tradeUiPlayerUpdatedCount, uint32_t tradeUiSessionStartCount,
                            uint32_t tradeUiSessionUpdatedCount, uint32_t tradeUiLastSessionStartState,
                            uint32_t tradeUiLastSessionStartPlayerNumber,
                            const char* partnerItemsJson = "[]",
                            const char* recentChatJson = "[]") {
    char path[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&WriteTradeHelperStatus), &hSelf);
    GetModuleFileNameA(hSelf, path, MAX_PATH);
    char* slash = strrchr(path, '\\');
    if (slash) *(slash + 1) = '\0';
    strcat_s(path, "trade_helper_status.json");

    char buf[16384];
    sprintf_s(buf,
              "{\"map_id\":%u,\"region\":%u,\"district\":%u,\"my_id\":%u,\"x\":%.1f,\"y\":%.1f,\"trade_flags\":%u,\"trade_open_count\":%u,\"last_open_flags\":%u,\"submit_attempt_count\":%u,\"accept_attempt_count\":%u,\"chat_send_attempt_count\":%u,\"whisper_send_attempt_count\":%u,\"last_chat_send_seq\":%u,\"last_whisper_send_seq\":%u,\"inventory_free_slots_total\":%u,\"helper_stackable_offer_model_id\":%u,\"helper_stackable_offer_quantity\":%u,\"helper_stackable_offer_total_quantity\":%u,\"helper_safe_singleton_offer_model_id\":%u,\"helper_safe_singleton_offer_total_quantity\":%u,\"player_gold\":%u,\"partner_gold\":%u,\"player_item_count\":%u,\"partner_item_count\":%u,\"ctos_total\":%u,\"ctos_submit\":%u,\"ctos_accept\":%u,\"ctos_cancel\":%u,\"ctos_add_item\":%u,\"partner_items\":%s,\"recent_chat\":%s,\"trade_partner_hook_hits\":%u,\"trade_partner_last_eax\":%u,\"trade_partner_last_ecx\":%u,\"trade_partner_last_edx\":%u,\"trade_ui_player_updated_count\":%u,\"trade_ui_session_start_count\":%u,\"trade_ui_session_updated_count\":%u,\"trade_ui_last_session_start_state\":%u,\"trade_ui_last_session_start_player_number\":%u}\n",
              mapId, region, district, myId, x, y, tradeFlags, tradeOpenCount, lastOpenFlags, submitAttemptCount, acceptAttemptCount,
              chatSendAttemptCount, whisperSendAttemptCount, lastChatSendSeq, lastWhisperSendSeq,
              inventoryFreeSlotsTotal, helperStackableOfferModelId, helperStackableOfferQuantity, helperStackableOfferTotalQuantity,
              helperSafeSingletonOfferModelId, helperSafeSingletonOfferTotalQuantity,
              playerGold, partnerGold, playerItemCount, partnerItemCount,
              ctoSPacketTotal, ctoSTradeSubmitCount, ctoSTradeAcceptCount, ctoSTradeCancelCount, ctoSTradeAddItemCount,
              partnerItemsJson ? partnerItemsJson : "[]",
              recentChatJson ? recentChatJson : "[]",
              tradePartnerHookHits, tradePartnerLastEax, tradePartnerLastEcx, tradePartnerLastEdx,
              tradeUiPlayerUpdatedCount, tradeUiSessionStartCount, tradeUiSessionUpdatedCount,
              tradeUiLastSessionStartState, tradeUiLastSessionStartPlayerNumber);

    HANDLE h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(h, buf, static_cast<DWORD>(strlen(buf)), &written, nullptr);
    CloseHandle(h);
}

void WriteConsumableHarnessStatus(const char* stage, const char* targetLabel, uint32_t mapId, uint32_t npcId,
                                  uint32_t merchantItemCount, uint32_t targetModelId, uint32_t targetItemId,
                                  uint32_t beforeCount, uint32_t afterCount, uint32_t success,
                                  const char* detail) {
    char dirPath[MAX_PATH];
    HMODULE hSelf = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&WriteConsumableHarnessStatus), &hSelf);
    GetModuleFileNameA(hSelf, dirPath, MAX_PATH);
    char* slash = strrchr(dirPath, '\\');
    if (slash) *(slash + 1) = '\0';

    const char* safeStage = stage ? stage : "";
    const char* safeTarget = targetLabel ? targetLabel : "";
    const char* safeDetail = detail ? detail : "";
    const DWORD pid = GetCurrentProcessId();
    static const DWORD runTag = GetTickCount();

    char buf[1152];
    sprintf_s(buf,
              "{\"pid\":%lu,\"run_tag\":%lu,\"stage\":\"%s\",\"target\":\"%s\",\"map_id\":%u,\"npc_id\":%u,\"merchant_item_count\":%u,\"target_model_id\":%u,\"target_item_id\":%u,\"before_count\":%u,\"after_count\":%u,\"success\":%u,\"detail\":\"%s\"}\n",
              pid, runTag, safeStage, safeTarget, mapId, npcId, merchantItemCount, targetModelId, targetItemId,
              beforeCount, afterCount, success, safeDetail);

    char statusPath[MAX_PATH];
    strcpy_s(statusPath, dirPath);
    strcat_s(statusPath, "consumable_harness_status.json");

    HANDLE h = CreateFileA(statusPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(h, buf, static_cast<DWORD>(strlen(buf)), &written, nullptr);
        CloseHandle(h);
    }

    char historyPath[MAX_PATH];
    strcpy_s(historyPath, dirPath);
    strcat_s(historyPath, "consumable_harness_history.jsonl");
    HANDLE hh = CreateFileA(historyPath, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hh != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(hh, buf, static_cast<DWORD>(strlen(buf)), &written, nullptr);
        CloseHandle(hh);
    }
}

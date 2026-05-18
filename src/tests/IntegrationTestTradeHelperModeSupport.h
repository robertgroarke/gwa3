bool PrepareTradeHelperModeWorld() {
    const uint32_t startMapId = ReadMapId();
    if (startMapId == 0) {
        IntSkip("Trade helper", "Not in game");
        return false;
    }

    if (!TradePartnerHook::Initialize()) {
        IntReport("  TradePartnerHook unavailable; helper will continue without partner-event telemetry");
    } else {
        TradePartnerHook::Reset();
    }

    const bool helperWorldReady = WaitFor("Helper world settle before travel", 5000, []() {
        return ReadMapId() > 0
            && ReadMyId() > 0
            && MapMgr::GetLoadingState() == 1
            && MapMgr::GetDistrict() != UINT32_MAX
            && MapMgr::GetDistrict() != 0;
    });
    IntReport("  Helper pre-travel settle: ready=%d map=%u region=%u district=%u myId=%u loading=%u",
              helperWorldReady ? 1 : 0,
              ReadMapId(),
              MapMgr::GetRegion(),
              MapMgr::GetDistrict(),
              ReadMyId(),
              MapMgr::GetLoadingState());
    AgentMgr::CancelAction();
    Sleep(750);

    if (startMapId != MapIds::LONGEYES_LEDGE || MapMgr::GetRegion() != kTradeTestRegion || MapMgr::GetDistrict() != kTradeTestDistrict) {
        IntReport("  Traveling helper to Longeye's Ledge Asia/Japan district %u...", kTradeTestDistrict);
        MapMgr::Travel(MapIds::LONGEYES_LEDGE, kTradeTestRegion, kTradeTestDistrict, kTradeTestLanguage);
        bool traveled = WaitFor("Helper reaches Longeye's Ledge preferred district", 20000, []() {
            return ReadMapId() == MapIds::LONGEYES_LEDGE
                && ReadMyId() > 0
                && MapMgr::GetRegion() == kTradeTestRegion
                && MapMgr::GetDistrict() == kTradeTestDistrict;
        });
        if (!traveled) {
            IntReport("  Preferred quiet Asia/Japan district did not load; falling back to Asia/Japan district %u", kTradeFallbackDistrict);
            MapMgr::Travel(MapIds::LONGEYES_LEDGE, kTradeTestRegion, kTradeFallbackDistrict, kTradeTestLanguage);
            traveled = WaitFor("Helper reaches Longeye's Ledge fallback district", 60000, []() {
                return ReadMapId() == MapIds::LONGEYES_LEDGE
                    && ReadMyId() > 0
                    && MapMgr::GetRegion() == kTradeTestRegion
                    && MapMgr::GetDistrict() == kTradeFallbackDistrict;
            });
        }
        IntCheck("Helper reached Longeye's Ledge", traveled);
        if (!traveled) {
            return false;
        }
    }
    IntReport("  Helper staying near spawn in quiet trade district");
    return true;
}

bool TryHandleTradeHelperChatSend(uint32_t& lastChatSendSeq) {
    const uint32_t chatSendSeq = ReadTradeHelperChatSendSequenceConfig();
    if (chatSendSeq == 0 || chatSendSeq == lastChatSendSeq) return false;

    char channelName[32] = {};
    char messageUtf8[256] = {};
    const bool haveChannel = ReadTradeHelperChatSendChannelConfig(channelName, sizeof(channelName));
    const bool haveMessage = ReadTradeHelperChatSendMessageConfig(messageUtf8, sizeof(messageUtf8));
    lastChatSendSeq = chatSendSeq;
    if (haveChannel && haveMessage) {
        wchar_t messageWide[256] = {};
        MultiByteToWideChar(CP_UTF8, 0, messageUtf8, -1, messageWide, _countof(messageWide) - 1);
        const wchar_t channelPrefix = MapTradeHelperChatChannelPrefix(channelName);
        const std::wstring messageWideCopy(messageWide);
        IntReport("  Helper sending chat seq=%u channel=%s message=%s",
                  chatSendSeq, channelName, messageUtf8);
        GameThread::Enqueue([messageWideCopy, channelPrefix]() {
            ChatMgr::SendChat(messageWideCopy.c_str(), channelPrefix);
        });
        return true;
    }

    IntReport("  Helper chat send seq=%u ignored because channel/message config was incomplete",
              chatSendSeq);
    return false;
}

bool TryHandleTradeHelperWhisperSend(uint32_t& lastWhisperSendSeq) {
    const uint32_t whisperSendSeq = ReadTradeHelperWhisperSendSequenceConfig();
    if (whisperSendSeq == 0 || whisperSendSeq == lastWhisperSendSeq) return false;

    char recipientUtf8[128] = {};
    char messageUtf8[256] = {};
    const bool haveRecipient = ReadTradeHelperWhisperSendRecipientConfig(recipientUtf8, sizeof(recipientUtf8));
    const bool haveMessage = ReadTradeHelperWhisperSendMessageConfig(messageUtf8, sizeof(messageUtf8));
    lastWhisperSendSeq = whisperSendSeq;
    if (haveRecipient && haveMessage) {
        wchar_t recipientWide[128] = {};
        wchar_t messageWide[256] = {};
        MultiByteToWideChar(CP_UTF8, 0, recipientUtf8, -1, recipientWide, _countof(recipientWide) - 1);
        MultiByteToWideChar(CP_UTF8, 0, messageUtf8, -1, messageWide, _countof(messageWide) - 1);
        const std::wstring recipientWideCopy(recipientWide);
        const std::wstring messageWideCopy(messageWide);
        IntReport("  Helper sending whisper seq=%u recipient=%s message=%s",
                  whisperSendSeq, recipientUtf8, messageUtf8);
        GameThread::Enqueue([recipientWideCopy, messageWideCopy]() {
            ChatMgr::SendWhisper(recipientWideCopy.c_str(), messageWideCopy.c_str());
        });
        return true;
    }

    IntReport("  Helper whisper send seq=%u ignored because recipient/message config was incomplete",
              whisperSendSeq);
    return false;
}

struct TradeHelperMoveState {
    uint32_t lastSeq = 0;
    DWORD lastIssuedAt = 0;
    float targetX = 0.0f;
    float targetY = 0.0f;
    bool pending = false;
};

struct TradeHelperOpenTradeState {
    DWORD openedAt = 0;
    DWORD lastSubmitAttemptAt = 0;
    DWORD lastAcceptAttemptAt = 0;
    DWORD flagsLastChangedAt = 0;
    uint32_t lastOpenFlags = 0;
    uint32_t lastObservedFlags = 0;
    bool submitted = false;
    bool accepted = false;
    bool offeredPrimaryItem = false;
    bool offeredSecondaryItem = false;
    bool staleCancelQueued = false;
    uint32_t submitGold = 0;
    uint32_t offerModel = 0;
    uint32_t offerQuantity = 0;
    uint32_t offerModel2 = 0;
    uint32_t offerQuantity2 = 0;
    DWORD lastOfferQueuedAt = 0;
};

void UpdateTradeHelperOpenTradeState(TradeHelperOpenTradeState& tradeState,
                                     uint32_t tradeFlags,
                                     bool tradeOpen,
                                     DWORD now,
                                     uint32_t& tradeOpenCount) {
    if (tradeFlags != tradeState.lastObservedFlags) {
        tradeState.flagsLastChangedAt = now;
        tradeState.lastObservedFlags = tradeFlags;
        IntReport("  Helper observed trade flags change -> %u (open=%u age=%u ms accepted=%u submitted=%u)",
                  tradeFlags,
                  tradeOpen ? 1u : 0u,
                  tradeState.openedAt != 0 ? (now - tradeState.openedAt) : 0u,
                  tradeState.accepted ? 1u : 0u,
                  tradeState.submitted ? 1u : 0u);
    }

    if (tradeOpen && tradeState.openedAt == 0) {
        tradeState.openedAt = now;
        tradeState.lastSubmitAttemptAt = 0;
        tradeState.lastAcceptAttemptAt = 0;
        tradeState.flagsLastChangedAt = now;
        ++tradeOpenCount;
        tradeState.lastOpenFlags = tradeFlags;
        tradeState.submitted = false;
        tradeState.accepted = false;
        tradeState.offeredPrimaryItem = false;
        tradeState.offeredSecondaryItem = false;
        tradeState.staleCancelQueued = false;
        tradeState.submitGold = ReadTradeHelperSubmitGoldConfig();
        tradeState.offerModel = ReadTradeHelperOfferItemModelConfig();
        tradeState.offerQuantity = ReadTradeHelperOfferItemQuantityConfig();
        tradeState.offerModel2 = ReadTradeHelperOfferItemModelConfig2();
        tradeState.offerQuantity2 = ReadTradeHelperOfferItemQuantityConfig2();
        tradeState.lastOfferQueuedAt = 0;
        IntReport("  Helper observed player trade open (flags=%u offerModel=%u offerQuantity=%u offerModel2=%u offerQuantity2=%u)",
                  tradeFlags, tradeState.offerModel, tradeState.offerQuantity, tradeState.offerModel2, tradeState.offerQuantity2);
    }

    if (!tradeOpen) {
        tradeState.openedAt = 0;
        tradeState.lastSubmitAttemptAt = 0;
        tradeState.lastAcceptAttemptAt = 0;
        tradeState.flagsLastChangedAt = now;
        tradeState.lastObservedFlags = 0;
        tradeState.submitted = false;
        tradeState.accepted = false;
        tradeState.offeredPrimaryItem = false;
        tradeState.offeredSecondaryItem = false;
        tradeState.staleCancelQueued = false;
        tradeState.submitGold = 0;
        tradeState.offerModel = 0;
        tradeState.offerQuantity = 0;
        tradeState.offerModel2 = 0;
        tradeState.offerQuantity2 = 0;
        tradeState.lastOfferQueuedAt = 0;
    }
}

void UpdateTradeHelperMoveRequest(TradeHelperMoveState& moveState) {
    const uint32_t moveSeq = ReadTradeHelperMoveSequenceConfig();
    if (moveSeq == 0 || moveSeq == moveState.lastSeq) return;

    float targetX = 0.0f;
    float targetY = 0.0f;
    if (ReadTradeHelperMoveTargetConfig(targetX, targetY)) {
        moveState.targetX = targetX;
        moveState.targetY = targetY;
        moveState.pending = true;
        moveState.lastSeq = moveSeq;
        moveState.lastIssuedAt = 0;
        IntReport("  Helper received rendezvous move request seq=%u target=(%.1f, %.1f)",
                  moveSeq, moveState.targetX, moveState.targetY);
    }
}

void TickTradeHelperMovement(TradeHelperMoveState& moveState, DWORD now, float x, float y) {
    if (!moveState.pending) return;

    const float dx = moveState.targetX - x;
    const float dy = moveState.targetY - y;
    const float distSq = dx * dx + dy * dy;
    if (distSq <= 100.0f * 100.0f) {
        moveState.pending = false;
        IntReport("  Helper reached rendezvous target seq=%u pos=(%.1f, %.1f)", moveState.lastSeq, x, y);
        return;
    }

    if (moveState.lastIssuedAt == 0 || now - moveState.lastIssuedAt >= 1500) {
        const float moveX = moveState.targetX;
        const float moveY = moveState.targetY;
        IntReport("  Helper moving toward rendezvous seq=%u current=(%.1f, %.1f) target=(%.1f, %.1f)",
                  moveState.lastSeq, x, y, moveX, moveY);
        GameThread::EnqueuePost([moveX, moveY]() {
            AgentMgr::Move(moveX, moveY);
        });
        moveState.lastIssuedAt = now;
    }
}

void QueueTradeHelperConfiguredOffer(uint32_t slotIndex,
                                     uint32_t modelId,
                                     uint32_t requestedQuantity,
                                     uint32_t tradeFlags,
                                     DWORD now,
                                     DWORD& lastOfferQueuedAt,
                                     bool& offeredFlag) {
    const uint32_t itemId = FindHelperInventoryItemByModel(modelId);
    if (itemId > 0) {
        uint32_t availableQuantity = 1;
        if (auto* item = ItemMgr::GetItemById(itemId)) {
            availableQuantity = item->quantity;
        }
        uint32_t quantity = availableQuantity;
        if (requestedQuantity > 0 && requestedQuantity < quantity) {
            quantity = requestedQuantity;
        }
        if (quantity == 0) {
            quantity = 1;
        }
        IntReport("  Helper auto-offering slot=%u item=%u model=%u qty=%u requested=%u available=%u mode=packet_offer (flags=%u)",
                  slotIndex + 1u, itemId, modelId, quantity, requestedQuantity,
                  availableQuantity, tradeFlags);
        GameThread::Enqueue([itemId, quantity]() {
            TradeMgr::OfferItemPacket(itemId, quantity);
        });
    } else {
        IntReport("  Helper offer_item_model_id[%u]=%u not found in inventory for requested qty=%u",
                  slotIndex + 1u, modelId, requestedQuantity);
    }
    offeredFlag = true;
    lastOfferQueuedAt = now;
}

void MaybeQueueTradeHelperConfiguredOffers(TradeHelperOpenTradeState& tradeState,
                                           bool tradeOpen,
                                           bool autoSubmitEnabled,
                                           uint32_t tradeFlags,
                                           DWORD now) {
    if (!tradeOpen || !autoSubmitEnabled || tradeState.openedAt == 0 || now - tradeState.openedAt <= 500) return;
    if (tradeState.lastOfferQueuedAt != 0 && now - tradeState.lastOfferQueuedAt < 500) return;

    if (tradeState.offerModel > 0 && !tradeState.offeredPrimaryItem) {
        QueueTradeHelperConfiguredOffer(0u, tradeState.offerModel, tradeState.offerQuantity,
                                        tradeFlags, now, tradeState.lastOfferQueuedAt, tradeState.offeredPrimaryItem);
    } else if (tradeState.offerModel2 > 0 && !tradeState.offeredSecondaryItem) {
        QueueTradeHelperConfiguredOffer(1u, tradeState.offerModel2, tradeState.offerQuantity2,
                                        tradeFlags, now, tradeState.lastOfferQueuedAt, tradeState.offeredSecondaryItem);
    }
}

bool AreTradeHelperConfiguredOffersHandled(uint32_t offerModelThisOpen,
                                           bool offeredPrimaryItemThisOpen,
                                           uint32_t offerModelThisOpen2,
                                           bool offeredSecondaryItemThisOpen) {
    return (offerModelThisOpen == 0 || offeredPrimaryItemThisOpen)
        && (offerModelThisOpen2 == 0 || offeredSecondaryItemThisOpen);
}

void MaybeSubmitTradeHelperOffer(bool tradeOpen,
                                 bool autoSubmitEnabled,
                                 DWORD tradeOpenedAt,
                                 DWORD now,
                                 DWORD lastOfferQueuedAt,
                                 bool allConfiguredOffersHandled,
                                 uint32_t submitGoldThisOpen,
                                 uint32_t tradeFlags,
                                 uint32_t& submitAttemptCount,
                                 bool& submittedThisOpen,
                                 DWORD& lastSubmitAttemptAt) {
    if (!tradeOpen || !autoSubmitEnabled || submittedThisOpen || tradeOpenedAt == 0 || now - tradeOpenedAt <= 750) return;
    if (!allConfiguredOffersHandled || (lastOfferQueuedAt != 0 && now - lastOfferQueuedAt < 500)) return;

    const uint32_t gold = submitGoldThisOpen;
    IntReport("  Helper auto-submitting offer gold=%u (flags=%u)", gold, tradeFlags);
    GameThread::Enqueue([gold]() { TradeMgr::SubmitOffer(gold); });
    ++submitAttemptCount;
    submittedThisOpen = true;
    lastSubmitAttemptAt = now;
}

void MaybeAcceptTradeHelperOffer(bool tradeOpen,
                                 bool autoAcceptEnabled,
                                 DWORD tradeOpenedAt,
                                 DWORD now,
                                 uint32_t tradeFlags,
                                 uint32_t& acceptAttemptCount,
                                 bool submittedThisOpen,
                                 bool& acceptedThisOpen,
                                 DWORD& lastAcceptAttemptAt) {
    const bool acceptReady = (tradeFlags & 0x2u) != 0u;
    if (!tradeOpen || !autoAcceptEnabled || !submittedThisOpen || acceptedThisOpen) return;
    if (tradeOpenedAt == 0 || now - tradeOpenedAt <= 1500 || !acceptReady) return;

    IntReport("  Helper auto-accepting incoming trade via TradeMgr::AcceptTrade after view+submit (flags=%u)", tradeFlags);
    GameThread::Enqueue([]() { TradeMgr::AcceptTrade(); });
    ++acceptAttemptCount;
    acceptedThisOpen = true;
    lastAcceptAttemptAt = now;
}

void MaybeCancelStaleTradeHelperOffer(bool tradeOpen,
                                      DWORD tradeOpenedAt,
                                      DWORD now,
                                      uint32_t tradeFlags,
                                      DWORD tradeFlagsLastChangedAt,
                                      bool acceptedThisOpen,
                                      DWORD lastAcceptAttemptAt,
                                      bool& staleCancelQueuedThisOpen) {
    if (!tradeOpen || staleCancelQueuedThisOpen) return;

    if (acceptedThisOpen && lastAcceptAttemptAt != 0 && now - lastAcceptAttemptAt > 4000) {
        IntReport("  Helper forcing sender-thread trade cancel after accept stall %u ms (flags=%u unchanged=%u ms)",
                  now - lastAcceptAttemptAt,
                  tradeFlags,
                  tradeFlagsLastChangedAt != 0 ? (now - tradeFlagsLastChangedAt) : 0u);
        CtoS::TradeCancelThreaded();
        staleCancelQueuedThisOpen = true;
        return;
    }

    if (tradeOpenedAt != 0 && now - tradeOpenedAt > 20000) {
        IntReport("  Helper forcing sender-thread cancel for stale trade after 20s (flags=%u unchanged=%u ms)",
                  tradeFlags,
                  tradeFlagsLastChangedAt != 0 ? (now - tradeFlagsLastChangedAt) : 0u);
        CtoS::TradeCancelThreaded();
        staleCancelQueuedThisOpen = true;
    }
}

void TickTradeHelperTradeActions(TradeHelperOpenTradeState& tradeState,
                                 bool tradeOpen,
                                 bool autoSubmitEnabled,
                                 bool autoAcceptEnabled,
                                 uint32_t tradeFlags,
                                 DWORD now,
                                 uint32_t& submitAttemptCount,
                                 uint32_t& acceptAttemptCount) {
    MaybeQueueTradeHelperConfiguredOffers(tradeState, tradeOpen, autoSubmitEnabled, tradeFlags, now);

    const bool allConfiguredOffersHandled = AreTradeHelperConfiguredOffersHandled(
        tradeState.offerModel, tradeState.offeredPrimaryItem,
        tradeState.offerModel2, tradeState.offeredSecondaryItem);
    MaybeSubmitTradeHelperOffer(
        tradeOpen, autoSubmitEnabled, tradeState.openedAt, now, tradeState.lastOfferQueuedAt,
        allConfiguredOffersHandled, tradeState.submitGold, tradeFlags,
        submitAttemptCount, tradeState.submitted, tradeState.lastSubmitAttemptAt);
    MaybeAcceptTradeHelperOffer(
        tradeOpen, autoAcceptEnabled, tradeState.openedAt, now, tradeFlags,
        acceptAttemptCount, tradeState.submitted, tradeState.accepted, tradeState.lastAcceptAttemptAt);
    MaybeCancelStaleTradeHelperOffer(
        tradeOpen, tradeState.openedAt, now, tradeFlags, tradeState.flagsLastChangedAt,
        tradeState.accepted, tradeState.lastAcceptAttemptAt, tradeState.staleCancelQueued);
}

void FormatTradeHelperPartnerItemsJson(char (&out)[512]) {
    strcpy_s(out, "[]");
    HelperPartnerItemInfo partnerItems[8] = {};
    const size_t partnerItemDetailCount = ReadTradePartnerItemsForHelper(partnerItems, 8);
    if (partnerItemDetailCount == 0) return;

    char* p = out;
    *p++ = '[';
    for (size_t pi = 0; pi < partnerItemDetailCount; ++pi) {
        if (pi > 0) *p++ = ',';
        p += sprintf_s(p, static_cast<size_t>(out + sizeof(out) - p),
                       "{\"item_id\":%u,\"model_id\":%u,\"quantity\":%u}",
                       partnerItems[pi].item_id, partnerItems[pi].model_id, partnerItems[pi].quantity);
    }
    *p++ = ']';
    *p = '\0';
}

uint32_t CountTradeHelperCtoSPacket(const CtoS::PacketTapSnapshot& ctosTap, uint32_t header) {
    for (uint32_t i = 0; i < _countof(ctosTap.headers); ++i) {
        if (ctosTap.headers[i] == header) {
            return ctosTap.counts[i];
        }
    }
    return 0u;
}

void WriteTradeHelperModeStatusAndHeartbeat(float x,
                                            float y,
                                            uint32_t tradeFlags,
                                            uint32_t tradeOpenCount,
                                            uint32_t lastOpenFlags,
                                            uint32_t submitAttemptCount,
                                            uint32_t acceptAttemptCount,
                                            uint32_t chatSendAttemptCount,
                                            uint32_t whisperSendAttemptCount,
                                            uint32_t lastChatSendSeq,
                                            uint32_t lastWhisperSendSeq,
                                            DWORD now,
                                            DWORD& lastLog) {
    const uint32_t mapId = ReadMapId();
    const uint32_t region = MapMgr::GetRegion();
    const uint32_t district = MapMgr::GetDistrict();
    uint32_t playerGold = 0;
    uint32_t partnerGold = 0;
    uint32_t playerItemCount = 0;
    uint32_t partnerItemCount = 0;
    ReadTradeStateForHelper(playerGold, partnerGold, playerItemCount, partnerItemCount);

    const uint32_t tradePartnerHookHits = TradePartnerHook::GetHitCount();
    const uint32_t tradePartnerLastEax = TradePartnerHook::GetLastEax();
    const uint32_t tradePartnerLastEcx = TradePartnerHook::GetLastEcx();
    const uint32_t tradePartnerLastEdx = TradePartnerHook::GetLastEdx();
    const uint32_t tradeUiPlayerUpdatedCount = TradeMgr::GetTradeUiPlayerUpdatedCount();
    const uint32_t tradeUiSessionStartCount = TradeMgr::GetTradeUiSessionStartCount();
    const uint32_t tradeUiSessionUpdatedCount = TradeMgr::GetTradeUiSessionUpdatedCount();
    const uint32_t tradeUiLastSessionStartState = TradeMgr::GetTradeUiLastSessionStartState();
    const uint32_t tradeUiLastSessionStartPlayerNumber = TradeMgr::GetTradeUiLastSessionStartPlayerNumber();

    char partnerItemsJson[512] = "[]";
    FormatTradeHelperPartnerItemsJson(partnerItemsJson);
    const auto ctosTap = CtoS::GetPacketTapSnapshot();
    const uint32_t inventoryFreeSlotsTotal = CountHelperInventoryFreeSlots();
    uint32_t helperStackableOfferModelId = 0;
    uint32_t helperStackableOfferQuantity = 0;
    FindHelperStackableInventoryCandidate(&helperStackableOfferModelId, &helperStackableOfferQuantity);
    const uint32_t helperStackableOfferTotalQuantity = CountHelperInventoryModelQuantity(helperStackableOfferModelId);
    uint32_t helperSafeSingletonOfferModelId = 0;
    FindHelperSafeSingletonInventoryCandidate(&helperSafeSingletonOfferModelId);
    const uint32_t helperSafeSingletonOfferTotalQuantity = CountHelperInventoryModelQuantity(helperSafeSingletonOfferModelId);
    const std::string recentChatJson = BuildRecentHelperChatJson();

    WriteTradeHelperStatus(mapId, region, district, ReadMyId(), x, y, tradeFlags, tradeOpenCount, lastOpenFlags,
                         submitAttemptCount, acceptAttemptCount,
                         chatSendAttemptCount, whisperSendAttemptCount,
                         lastChatSendSeq, lastWhisperSendSeq,
                         inventoryFreeSlotsTotal,
                         helperStackableOfferModelId, helperStackableOfferQuantity, helperStackableOfferTotalQuantity,
                         helperSafeSingletonOfferModelId, helperSafeSingletonOfferTotalQuantity,
                         playerGold, partnerGold, playerItemCount, partnerItemCount,
                         ctosTap.total_packets,
                         CountTradeHelperCtoSPacket(ctosTap, Packets::TRADE_SUBMIT_OFFER),
                         CountTradeHelperCtoSPacket(ctosTap, Packets::TRADE_ACCEPT),
                         CountTradeHelperCtoSPacket(ctosTap, Packets::TRADE_CANCEL),
                         CountTradeHelperCtoSPacket(ctosTap, Packets::TRADE_ADD_ITEM),
                         tradePartnerHookHits, tradePartnerLastEax, tradePartnerLastEcx, tradePartnerLastEdx,
                         tradeUiPlayerUpdatedCount, tradeUiSessionStartCount, tradeUiSessionUpdatedCount,
                         tradeUiLastSessionStartState, tradeUiLastSessionStartPlayerNumber,
                         partnerItemsJson, recentChatJson.c_str());

    if (now - lastLog >= 3000) {
        IntReport("  Helper heartbeat: map=%u region=%u district=%u myId=%u pos=(%.1f, %.1f) tradeFlags=%u",
                  mapId, region, district, ReadMyId(), x, y, tradeFlags);
        IntReport("    TradePartnerHook: hits=%u eax=%u ecx=%u edx=%u",
                  tradePartnerHookHits, tradePartnerLastEax, tradePartnerLastEcx, tradePartnerLastEdx);
        IntReport("    TradeUI: playerUpdated=%u sessionStart=%u sessionUpdated=%u lastState=%u lastPlayer=%u",
                  tradeUiPlayerUpdatedCount, tradeUiSessionStartCount, tradeUiSessionUpdatedCount,
                  tradeUiLastSessionStartState, tradeUiLastSessionStartPlayerNumber);
        lastLog = now;
    }
}

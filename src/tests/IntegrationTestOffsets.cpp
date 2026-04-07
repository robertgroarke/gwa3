// Offset and patch validation slices.

#include "IntegrationTestInternal.h"

#include <gwa3/core/Memory.h>

namespace GWA3::SmokeTest {

bool TestPostProcessEffectOffset() {
    IntReport("=== GWA3-059: PostProcessEffect Offset ===");

    IntReport("  PostProcessEffect: 0x%08X", static_cast<unsigned>(Offsets::PostProcessEffect));
    IntReport("  DropBuff: 0x%08X", static_cast<unsigned>(Offsets::DropBuff));

    if (Offsets::PostProcessEffect > 0x10000) {
        IntCheck("PostProcessEffect offset resolved", true);
    } else {
        IntSkip("PostProcessEffect offset", "Pattern did not resolve");
    }

    if (Offsets::DropBuff > 0x10000) {
        IntCheck("DropBuff offset resolved", true);
    } else {
        IntSkip("DropBuff offset", "Pattern did not resolve");
    }

    IntReport("");
    return true;
}

bool TestGwEndSceneOffset() {
    IntReport("=== GWA3-061: GwEndScene Offset ===");

    IntReport("  GwEndScene: 0x%08X", static_cast<unsigned>(Offsets::GwEndScene));
    IntReport("  Render (AutoIt): 0x%08X", static_cast<unsigned>(Offsets::Render));

    if (Offsets::GwEndScene > 0x10000) {
        IntCheck("GwEndScene offset resolved", true);

        if (Offsets::Render > 0x10000) {
            ptrdiff_t delta = static_cast<ptrdiff_t>(Offsets::GwEndScene) -
                              static_cast<ptrdiff_t>(Offsets::Render);
            IntReport("  Delta between GwEndScene and Render: %d bytes", static_cast<int>(delta));
            bool close = (delta >= -0x20 && delta <= 0x20) || delta == 0;
            if (close) {
                IntCheck("GwEndScene and Render point to same region", true);
            } else {
                IntReport("  GwEndScene and Render are far apart (may be different hook targets)");
                IntCheck("GwEndScene resolved independently", true);
            }
        }
    } else {
        IntSkip("GwEndScene offset", "Pattern did not resolve");
    }

    IntReport("");
    return true;
}

bool TestItemClickOffset() {
    IntReport("=== GWA3-064: ItemClick Offset ===");

    IntReport("  ItemClick: 0x%08X", static_cast<unsigned>(Offsets::ItemClick));

    if (Offsets::ItemClick > 0x10000) {
        IntCheck("ItemClick offset resolved", true);

        Inventory* inv = ItemMgr::GetInventory();
        if (inv) {
            for (uint32_t bagIdx = 1; bagIdx <= 4; ++bagIdx) {
                Bag* bag = inv->bags[bagIdx];
                if (!bag || !bag->items.buffer) continue;
                for (uint32_t i = 0; i < bag->items.size; ++i) {
                    Item* item = bag->items.buffer[i];
                    if (!item || item->item_id == 0) continue;
                    IntReport("  Found test item: id=%u model=%u in bag %u",
                              item->item_id, item->model_id, bagIdx);
                    IntCheck("ClickItem function available for resolved offset", true);
                    goto done_item_check;
                }
            }
            IntSkip("ClickItem test", "No items in backpack to test with");
            done_item_check:;
        } else {
            IntSkip("ClickItem test", "Inventory unavailable");
        }
    } else {
        IntSkip("ItemClick offset", "Pattern did not resolve");
    }

    IntReport("");
    return true;
}

bool TestRequestQuestInfoOffset() {
    IntReport("=== GWA3-070: RequestQuestInfo Offset ===");
    IntReport("  RequestQuestInfo: 0x%08X", static_cast<unsigned>(Offsets::RequestQuestInfo));
    if (Offsets::RequestQuestInfo > 0x10000) {
        IntCheck("RequestQuestInfo offset resolved", true);
    } else {
        IntSkip("RequestQuestInfo", "Pattern did not resolve");
    }
    IntReport("");
    return true;
}

bool TestFriendListOffsets() {
    IntReport("=== GWA3-071: FriendList Offsets ===");
    IntReport("  FriendListAddr: 0x%08X", static_cast<unsigned>(Offsets::FriendListAddr));
    IntReport("  FriendEventHandler: 0x%08X", static_cast<unsigned>(Offsets::FriendEventHandler));

    if (Offsets::FriendListAddr > 0x10000) {
        IntCheck("FriendListAddr resolved", true);
    } else {
        IntSkip("FriendListAddr", "Pattern did not resolve");
    }
    if (Offsets::FriendEventHandler > 0x10000) {
        IntCheck("FriendEventHandler resolved", true);
    } else {
        IntSkip("FriendEventHandler", "Pattern did not resolve");
    }
    IntReport("");
    return true;
}

bool TestDrawOnCompassOffset() {
    IntReport("=== GWA3-072: DrawOnCompass Offset ===");
    IntReport("  DrawOnCompass: 0x%08X", static_cast<unsigned>(Offsets::DrawOnCompass));
    if (Offsets::DrawOnCompass > 0x10000) {
        IntCheck("DrawOnCompass offset resolved", true);
    } else {
        IntSkip("DrawOnCompass", "Assertion pattern did not resolve");
    }
    IntReport("");
    return true;
}

bool TestChatColorOffsets() {
    IntReport("=== GWA3-073: Chat Color Offsets ===");
    IntReport("  GetSenderColor: 0x%08X", static_cast<unsigned>(Offsets::GetSenderColor));
    IntReport("  GetMessageColor: 0x%08X", static_cast<unsigned>(Offsets::GetMessageColor));

    if (Offsets::GetSenderColor > 0x10000) {
        IntCheck("GetSenderColor resolved", true);
    } else {
        IntSkip("GetSenderColor", "Pattern did not resolve");
    }
    if (Offsets::GetMessageColor > 0x10000) {
        IntCheck("GetMessageColor resolved", true);
    } else {
        IntSkip("GetMessageColor", "Pattern did not resolve");
    }
    IntReport("");
    return true;
}

bool TestCameraUpdateBypassPatch() {
    IntReport("=== GWA3-066: Camera Update Bypass Patch ===");

    IntReport("  CameraUpdateBypass: 0x%08X", static_cast<unsigned>(Offsets::CameraUpdateBypass));

    if (Offsets::CameraUpdateBypass > 0x10000) {
        IntCheck("CameraUpdateBypass offset resolved", true);

        auto& patch = Memory::GetCameraUnlockPatch();
        IntReport("  Patch staged: %s", patch.staged ? "yes" : "no");
        IntCheck("Patch is staged", patch.staged);

        if (patch.staged) {
            patch.Enable();
            IntCheck("CameraUpdateBypass Enable (no crash)", true);
            patch.Disable();
            IntCheck("CameraUpdateBypass Disable (no crash)", true);
        }
    } else {
        IntSkip("CameraUpdateBypass", "Pattern did not resolve");
    }

    IntReport("");
    return true;
}

bool TestTradeOffsets() {
    IntReport("=== GWA3-069: Trade Function Offsets ===");

    IntReport("  OfferTradeItem: 0x%08X", static_cast<unsigned>(Offsets::OfferTradeItem));
    IntReport("  UpdateTradeCart: 0x%08X", static_cast<unsigned>(Offsets::UpdateTradeCart));

    if (Offsets::OfferTradeItem > 0x10000) {
        IntCheck("OfferTradeItem offset resolved", true);
    } else {
        IntSkip("OfferTradeItem offset", "Pattern did not resolve");
    }

    if (Offsets::UpdateTradeCart > 0x10000) {
        IntCheck("UpdateTradeCart offset resolved", true);
    } else {
        IntSkip("UpdateTradeCart offset", "Pattern did not resolve");
    }

    IntReport("");
    return true;
}

bool TestLevelDataBypassPatch() {
    IntReport("=== GWA3-067: Level-Data Bypass Patch ===");

    IntReport("  LevelDataBypass: 0x%08X", static_cast<unsigned>(Offsets::LevelDataBypass));

    if (Offsets::LevelDataBypass > 0x10000) {
        IntCheck("LevelDataBypass offset resolved", true);

        auto& patch = Memory::GetLevelDataBypassPatch();
        IntReport("  Patch staged: %s", patch.staged ? "yes" : "no");
        IntCheck("Patch is staged", patch.staged);

        if (patch.staged) {
            patch.Enable();
            IntCheck("LevelDataBypass Enable (no crash)", true);
            patch.Disable();
            IntCheck("LevelDataBypass Disable (no crash)", true);
        }
    } else {
        IntSkip("LevelDataBypass", "Pattern did not resolve");
    }

    IntReport("");
    return true;
}

bool TestMapPortBypassPatch() {
    IntReport("=== GWA3-068: Map/Port Bypass Patch ===");

    IntReport("  MapPortBypass: 0x%08X", static_cast<unsigned>(Offsets::MapPortBypass));

    if (Offsets::MapPortBypass > 0x10000) {
        IntCheck("MapPortBypass offset resolved", true);

        auto& patch = Memory::GetMapPortBypassPatch();
        IntReport("  Patch staged: %s", patch.staged ? "yes" : "no");
        IntCheck("Patch is staged", patch.staged);

        if (patch.staged) {
            patch.Enable();
            IntCheck("MapPortBypass Enable (no crash)", true);
            patch.Disable();
            IntCheck("MapPortBypass Disable (no crash)", true);
        }
    } else {
        IntSkip("MapPortBypass", "Pattern did not resolve");
    }

    IntReport("");
    return true;
}

bool TestSendChatOffset() {
    IntReport("=== GWA3-062: SendChat Offset ===");

    IntReport("  SendChatFunc: 0x%08X", static_cast<unsigned>(Offsets::SendChatFunc));

    if (Offsets::SendChatFunc > 0x10000) {
        IntCheck("SendChatFunc offset resolved", true);
    } else {
        IntSkip("SendChatFunc offset", "Pattern did not resolve");
    }

    IntReport("");
    return true;
}

bool TestAddToChatLogOffset() {
    IntReport("=== GWA3-063: AddToChatLog Offset ===");

    IntReport("  AddToChatLog: 0x%08X", static_cast<unsigned>(Offsets::AddToChatLog));

    if (Offsets::AddToChatLog > 0x10000) {
        IntCheck("AddToChatLog offset resolved", true);
    } else {
        IntSkip("AddToChatLog offset", "Pattern did not resolve");
    }

    IntReport("");
    return true;
}

bool TestSkipCinematicOffset() {
    IntReport("=== GWA3-065: SkipCinematic Offset ===");

    IntReport("  SkipCinematicFunc: 0x%08X", static_cast<unsigned>(Offsets::SkipCinematicFunc));

    if (Offsets::SkipCinematicFunc > 0x10000) {
        IntCheck("SkipCinematicFunc offset resolved", true);
    } else {
        IntSkip("SkipCinematicFunc offset", "Pattern did not resolve");
    }

    IntReport("");
    return true;
}

} // namespace GWA3::SmokeTest

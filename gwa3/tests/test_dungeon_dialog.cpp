#include <gwa3/bot/DungeonDialog.h>
#include <gwa3/testing/TestFramework.h>

#include <cstddef>

namespace GWA3::TestStubs::QuestMgr {

void ResetDialogs();
std::size_t DialogCount();
uint32_t DialogAt(std::size_t index);

} // namespace GWA3::TestStubs::QuestMgr

using namespace GWA3::Bot::DungeonDialog;

GWA3_TEST(dungeon_dialog_retry_repeats_single_dialog, {
    GWA3::TestStubs::QuestMgr::ResetDialogs();

    GWA3_ASSERT(SendDialogWithRetry(0x8101u, 3, 0u));
    GWA3_ASSERT_EQ(static_cast<unsigned>(GWA3::TestStubs::QuestMgr::DialogCount()), 3u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(0), 0x8101u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(2), 0x8101u);
})

GWA3_TEST(dungeon_dialog_sequence_preserves_order, {
    GWA3::TestStubs::QuestMgr::ResetDialogs();
    const uint32_t dialogs[] = {0x8101u, 0x832303u, 0x8101u, 0x832301u};

    GWA3_ASSERT(SendDialogSequence(dialogs, 4, 0u, 1));
    GWA3_ASSERT_EQ(static_cast<unsigned>(GWA3::TestStubs::QuestMgr::DialogCount()), 4u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(0), 0x8101u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(1), 0x832303u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(2), 0x8101u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(3), 0x832301u);
})

GWA3_TEST(dungeon_dialog_repeated_sequence_replays_full_loop, {
    GWA3::TestStubs::QuestMgr::ResetDialogs();
    const uint32_t dialogs[] = {0x8101u, 0x832A01u};

    GWA3_ASSERT(SendDialogSequenceRepeated(dialogs, 2, 3, 0u, 0u, 1));
    GWA3_ASSERT_EQ(static_cast<unsigned>(GWA3::TestStubs::QuestMgr::DialogCount()), 6u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(0), 0x8101u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(1), 0x832A01u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(2), 0x8101u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(3), 0x832A01u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(4), 0x8101u);
    GWA3_ASSERT_EQ(GWA3::TestStubs::QuestMgr::DialogAt(5), 0x832A01u);
})

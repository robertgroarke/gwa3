namespace GWA3::Log {

void Info(const char*, ...) {
}

void Warn(const char*, ...) {
}

} // namespace GWA3::Log

namespace GWA3::TestStubs::TradeMgr {

void Reset() {
    g_last_transact_type = 0u;
    g_last_transact_quantity = 0u;
    g_last_transact_item_id = 0u;
    g_transact_count = 0u;
    g_test_merchant_item_count = 0u;
    g_last_buy_materials_model_id = 0u;
    g_last_buy_materials_quantity = 0u;
}

void SetMerchantItemCount(uint32_t count) {
    g_test_merchant_item_count = count;
}

uint32_t LastTransactType() {
    return g_last_transact_type;
}

uint32_t LastTransactQuantity() {
    return g_last_transact_quantity;
}

uint32_t LastTransactItemId() {
    return g_last_transact_item_id;
}

uint32_t TransactCount() {
    return g_transact_count;
}

uint32_t LastBuyMaterialsModelId() {
    return g_last_buy_materials_model_id;
}

uint32_t LastBuyMaterialsQuantity() {
    return g_last_buy_materials_quantity;
}

} // namespace GWA3::TestStubs::TradeMgr

namespace GWA3::TradeMgr {

void BuyMaterials(uint32_t modelId, uint32_t quantity) {
    g_last_buy_materials_model_id = modelId;
    g_last_buy_materials_quantity = quantity;
}

uint32_t GetMerchantItemCount() {
    return g_test_merchant_item_count;
}

void TransactItems(uint32_t type, uint32_t quantity, uint32_t itemId) {
    g_last_transact_type = type;
    g_last_transact_quantity = quantity;
    g_last_transact_item_id = itemId;
    ++g_transact_count;
    GWA3::ItemMgr::DropItem(itemId);
}

} // namespace GWA3::TradeMgr

namespace GWA3::MerchantMgr {

uint32_t GetMerchantItemCount() {
    return g_test_merchant_item_count;
}

void TransactItems(uint32_t type, uint32_t quantity, uint32_t itemId) {
    g_last_transact_type = type;
    g_last_transact_quantity = quantity;
    g_last_transact_item_id = itemId;
    ++g_transact_count;
    GWA3::ItemMgr::DropItem(itemId);
}

} // namespace GWA3::MerchantMgr

namespace GWA3::TestStubs::UIMgr {

void Reset() {
    g_test_visible_frame_hash = 0u;
    g_test_frame_visible = false;
    g_test_action_key_down_result = false;
    g_test_perform_ui_action_result = false;
    g_last_action_key_down = 0u;
    g_action_key_down_count = 0u;
    g_last_perform_ui_action = 0u;
    g_perform_ui_action_count = 0u;
    g_last_perform_ui_action_direct = 0u;
    g_perform_ui_action_direct_count = 0u;
}

void SetFrameVisible(uint32_t hash, bool visible) {
    g_test_visible_frame_hash = hash;
    g_test_frame_visible = visible;
}

void SetActionKeyDownResult(bool result) {
    g_test_action_key_down_result = result;
}

void SetPerformUiActionResult(bool result) {
    g_test_perform_ui_action_result = result;
}

uint32_t LastActionKeyDown() {
    return g_last_action_key_down;
}

uint32_t ActionKeyDownCount() {
    return g_action_key_down_count;
}

uint32_t LastPerformUiAction() {
    return g_last_perform_ui_action;
}

uint32_t PerformUiActionCount() {
    return g_perform_ui_action_count;
}

uint32_t LastPerformUiActionDirect() {
    return g_last_perform_ui_action_direct;
}

uint32_t PerformUiActionDirectCount() {
    return g_perform_ui_action_direct_count;
}

} // namespace GWA3::TestStubs::UIMgr

namespace GWA3::UIMgr {

bool IsFrameVisible(uint32_t hash) {
    return g_test_frame_visible && g_test_visible_frame_hash == hash;
}

bool ActionKeyDown(uint32_t action) {
    g_last_action_key_down = action;
    ++g_action_key_down_count;
    return g_test_action_key_down_result;
}

bool ActionKeyPress(uint32_t action) {
    g_last_action_key_down = action;
    ++g_action_key_down_count;
    return g_test_action_key_down_result;
}

bool PerformUiAction(uint32_t action) {
    g_last_perform_ui_action = action;
    ++g_perform_ui_action_count;
    return g_test_perform_ui_action_result;
}

bool PerformUiActionDirect(uint32_t action) {
    g_last_perform_ui_action_direct = action;
    ++g_perform_ui_action_direct_count;
    return g_test_perform_ui_action_result;
}

bool PerformUiActionWithFlag(uint32_t action, uint32_t /*flag*/) {
    return PerformUiAction(action);
}

bool PerformUiActionDirectWithFlag(uint32_t action, uint32_t /*flag*/) {
    return PerformUiActionDirect(action);
}

} // namespace GWA3::UIMgr

namespace GWA3::TestStubs::CtoS {

void Reset() {
    g_send_packet_count = 0u;
    g_last_send_packet_size = 0u;
    g_last_send_packet_header = 0u;
    g_last_send_packet_arg1 = 0u;
    g_last_send_packet_arg2 = 0u;
}

uint32_t SendPacketCount() {
    return g_send_packet_count;
}

uint32_t LastSendPacketHeader() {
    return g_last_send_packet_header;
}

uint32_t LastSendPacketArg1() {
    return g_last_send_packet_arg1;
}

uint32_t LastSendPacketArg2() {
    return g_last_send_packet_arg2;
}

} // namespace GWA3::TestStubs::CtoS

namespace GWA3::CtoS {

bool IsBotshubQueueIdle() {
    return g_test_botshub_queue_idle;
}

void SuspendEngineHook() {
}

void ResumeEngineHook() {
}

void SendPacket(uint32_t size, uint32_t header, ...) {
    g_last_send_packet_size = size;
    g_last_send_packet_header = header;
    ++g_send_packet_count;

    va_list args;
    va_start(args, header);
    g_last_send_packet_arg1 = va_arg(args, uint32_t);
    g_last_send_packet_arg2 = va_arg(args, uint32_t);
    va_end(args);
}

void SendPacketDirect(uint32_t size, uint32_t header, ...) {
    g_last_send_packet_size = size;
    g_last_send_packet_header = header;
    ++g_send_packet_count;

    va_list args;
    va_start(args, header);
    g_last_send_packet_arg1 = va_arg(args, uint32_t);
    g_last_send_packet_arg2 = va_arg(args, uint32_t);
    va_end(args);

    if (header == GWA3::Packets::INTERACT_NPC && g_test_auto_open_dialog_on_npc_interact) {
        g_test_dialog_open = true;
        g_test_dialog_sender_id = g_last_send_packet_arg1;
        g_test_dialog_button_count = g_test_auto_dialog_button_count;
    }
}

} // namespace GWA3::CtoS

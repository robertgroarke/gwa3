#include "ActionExecutorInternal.h"

#include <gwa3/core/GameThread.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/ItemMgr.h>

using json = nlohmann::json;

namespace GWA3::LLM::ActionExecutor {

    namespace {

        ActionResult HandlePickUpItem(const json& p) {
            if (!p.contains("agent_id")) return MakeError("missing agent_id");
            uint32_t id = p["agent_id"].get<uint32_t>();
            if (!AgentMgr::GetAgentExists(id)) return MakeError("agent_not_found");
            GWA3::GameThread::Enqueue([id]() { ItemMgr::PickUpItem(id); });
            return MakeOk();
        }

        ActionResult HandleUseItem(const json& p) {
            if (!p.contains("item_id")) return MakeError("missing item_id");
            uint32_t id = p["item_id"].get<uint32_t>();
            if (!ItemMgr::GetItemById(id)) return MakeError("item_not_found");
            GWA3::GameThread::Enqueue([id]() { ItemMgr::UseItem(id); });
            return MakeOk();
        }

        ActionResult HandleEquipItem(const json& p) {
            if (!p.contains("item_id")) return MakeError("missing item_id");
            uint32_t id = p["item_id"].get<uint32_t>();
            if (!ItemMgr::GetItemById(id)) return MakeError("item_not_found");
            GWA3::GameThread::Enqueue([id]() { ItemMgr::EquipItem(id); });
            return MakeOk();
        }

        ActionResult HandleDropItem(const json& p) {
            if (!p.contains("item_id")) return MakeError("missing item_id");
            uint32_t id = p["item_id"].get<uint32_t>();
            if (!ItemMgr::GetItemById(id)) return MakeError("item_not_found");
            GWA3::GameThread::Enqueue([id]() { ItemMgr::DropItem(id); });
            return MakeOk();
        }

        ActionResult HandleMoveItem(const json& p) {
            if (!p.contains("item_id") || !p.contains("bag_id") || !p.contains("slot"))
                return MakeError("missing item_id, bag_id, or slot");
            uint32_t itemId = p["item_id"].get<uint32_t>();
            uint32_t bagId = p["bag_id"].get<uint32_t>();
            uint32_t slot = p["slot"].get<uint32_t>();
            if (!ItemMgr::GetItemById(itemId)) return MakeError("item_not_found");
            GWA3::GameThread::Enqueue([itemId, bagId, slot]() { ItemMgr::MoveItem(itemId, bagId, slot); });
            return MakeOk();
        }

        ActionResult HandleIdentifyItem(const json& p) {
            if (!p.contains("item_id") || !p.contains("kit_id"))
                return MakeError("missing item_id or kit_id");
            uint32_t itemId = p["item_id"].get<uint32_t>();
            uint32_t kitId = p["kit_id"].get<uint32_t>();
            if (!ItemMgr::GetItemById(itemId)) return MakeError("item_not_found");
            if (!ItemMgr::GetItemById(kitId)) return MakeError("kit_not_found");
            GWA3::GameThread::Enqueue([itemId, kitId]() { ItemMgr::IdentifyItem(itemId, kitId); });
            return MakeOk();
        }

        ActionResult HandleSalvageStart(const json& p) {
            if (!p.contains("item_id") || !p.contains("kit_id"))
                return MakeError("missing item_id or kit_id");
            uint32_t itemId = p["item_id"].get<uint32_t>();
            uint32_t kitId = p["kit_id"].get<uint32_t>();
            if (!ItemMgr::GetItemById(itemId)) return MakeError("item_not_found");
            if (!ItemMgr::GetItemById(kitId)) return MakeError("kit_not_found");
            GWA3::GameThread::Enqueue([kitId, itemId]() { ItemMgr::SalvageSessionOpen(kitId, itemId); });
            return MakeOk();
        }

        ActionResult HandleSalvageMaterials(const json&) {
            GWA3::GameThread::Enqueue([]() { ItemMgr::SalvageMaterials(); });
            return MakeOk();
        }

        ActionResult HandleSalvageDone(const json&) {
            GWA3::GameThread::Enqueue([]() { ItemMgr::SalvageSessionDone(); });
            return MakeOk();
        }

    } // namespace

    void RegisterItemActions(ActionDispatchTable& dispatch) {
        dispatch["pick_up_item"] = HandlePickUpItem;
        dispatch["use_item"] = HandleUseItem;
        dispatch["equip_item"] = HandleEquipItem;
        dispatch["drop_item"] = HandleDropItem;
        dispatch["move_item"] = HandleMoveItem;

        dispatch["identify_item"] = HandleIdentifyItem;
        dispatch["salvage_start"] = HandleSalvageStart;
        dispatch["salvage_materials"] = HandleSalvageMaterials;
        dispatch["salvage_done"] = HandleSalvageDone;
    }

} // namespace GWA3::LLM::ActionExecutor

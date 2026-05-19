#include "ActionExecutorInternal.h"

#include <gwa3/core/GameThread.h>
#include <gwa3/core/Log.h>
#include <gwa3/managers/ChatMgr.h>
#include <gwa3/managers/ItemMgr.h>
#include <bots/common/BotFramework.h>

#include <Windows.h>

#include <string>

using json = nlohmann::json;

namespace GWA3::LLM::ActionExecutor {

    namespace {

        ActionResult HandleSendChat(const json& p) {
            if (!p.contains("message") || !p.contains("channel"))
                return MakeError("missing message or channel");
            std::string message = p["message"].get<std::string>();
            std::string channel = p["channel"].get<std::string>();
            if (message.empty()) return MakeError("empty_message");

            wchar_t wMsg[256] = {};
            MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, wMsg, 255);
            wchar_t ch = '!'; // default to all chat
            if (channel == "team" || channel == "party") ch = '#';
            else if (channel == "guild") ch = '@';
            else if (channel == "trade") ch = '$';
            else if (channel == "all") ch = '!';

            GWA3::GameThread::Enqueue([wMsg, ch]() {
                ChatMgr::SendChat(wMsg, ch);
            });
            return MakeOk();
        }

        ActionResult HandleSendWhisper(const json& p) {
            if (!p.contains("recipient") || !p.contains("message"))
                return MakeError("missing recipient or message");
            std::string recipient = p["recipient"].get<std::string>();
            std::string message = p["message"].get<std::string>();
            if (recipient.empty()) return MakeError("empty_recipient");
            if (message.empty()) return MakeError("empty_message");

            wchar_t wRecipient[128] = {};
            wchar_t wMsg[256] = {};
            MultiByteToWideChar(CP_UTF8, 0, recipient.c_str(), -1, wRecipient, 127);
            MultiByteToWideChar(CP_UTF8, 0, message.c_str(), -1, wMsg, 255);

            GWA3::GameThread::Enqueue([wRecipient, wMsg]() {
                ChatMgr::SendWhisper(wRecipient, wMsg);
            });
            return MakeOk();
        }

        ActionResult HandleDropGold(const json& p) {
            if (!p.contains("amount")) return MakeError("missing amount");
            uint32_t amount = p["amount"].get<uint32_t>();
            GWA3::GameThread::Enqueue([amount]() { ItemMgr::DropGold(amount); });
            return MakeOk();
        }

        ActionResult HandleSetCombatMode(const json& p) {
            if (!p.contains("mode")) return MakeError("missing mode");
            std::string mode = p["mode"].get<std::string>();
            auto& cfg = GWA3::Bot::GetConfig();
            if (mode == "builtin") {
                cfg.combat_mode = GWA3::Bot::CombatMode::Builtin;
            } else if (mode == "llm") {
                cfg.combat_mode = GWA3::Bot::CombatMode::LLM;
            } else {
                return MakeError("unknown_mode");
            }
            GWA3::Log::Info("[LLM-Action] Combat mode set to: %s", mode.c_str());
            return MakeOk();
        }

        ActionResult HandleSetBotState(const json& p) {
            if (!p.contains("state")) return MakeError("missing state");
            std::string stateName = p["state"].get<std::string>();

            GWA3::Bot::BotState target;
            if (stateName == "idle") target = GWA3::Bot::BotState::Idle;
            else if (stateName == "in_town") target = GWA3::Bot::BotState::InTown;
            else if (stateName == "traveling") target = GWA3::Bot::BotState::Traveling;
            else if (stateName == "in_dungeon") target = GWA3::Bot::BotState::InDungeon;
            else if (stateName == "looting") target = GWA3::Bot::BotState::Looting;
            else if (stateName == "awaiting_return") target = GWA3::Bot::BotState::AwaitingReturn;
            else if (stateName == "merchant") target = GWA3::Bot::BotState::Merchant;
            else if (stateName == "maintenance") target = GWA3::Bot::BotState::Maintenance;
            else if (stateName == "llm_controlled") target = GWA3::Bot::BotState::LLMControlled;
            else return MakeError("unknown_state");

            GWA3::Bot::SetState(target);
            GWA3::Log::Info("[LLM-Action] Bot state overridden to: %s", stateName.c_str());
            return MakeOk();
        }

        ActionResult HandleResign(const json&) {
            wchar_t msg[] = L"/resign";
            GWA3::GameThread::Enqueue([msg]() {
                ChatMgr::SendChat(msg, L'/');
            });
            return MakeOk();
        }

        ActionResult HandleWait(const json& p) {
            (void)p;
            return MakeOk();
        }

    } // namespace

    void RegisterBotControlActions(ActionDispatchTable& dispatch) {
        dispatch["set_bot_state"] = HandleSetBotState;
        dispatch["set_combat_mode"] = HandleSetCombatMode;
    }

    void RegisterUtilityActions(ActionDispatchTable& dispatch) {
        dispatch["send_chat"] = HandleSendChat;
        dispatch["send_whisper"] = HandleSendWhisper;
        dispatch["drop_gold"] = HandleDropGold;
        dispatch["resign"] = HandleResign;
        dispatch["wait"] = HandleWait;
    }

} // namespace GWA3::LLM::ActionExecutor

#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace GWA3::Bot {

    enum class BotState {
        Idle,
        CharSelect,
        InTown,
        Traveling,
        InDungeon,
        Looting,
        Merchant,
        Maintenance,
        Error,
        Stopping,
        LLMControlled
    };

    struct BotConfig {
        bool use_consets = false;
        bool use_stones = false;
        bool hard_mode = true;
        bool disable_rendering = false;
        bool open_chests = true;
        bool pickup_golds = true;
        bool auto_salvage = true;
        uint32_t hero_ids[7] = {0};
        uint32_t skill_template[8] = {0};
        uint32_t target_map_id = 0;
        uint32_t outpost_map_id = 0;
        std::string hero_config_file;
        std::string bot_module_name;
    };

    // State handler: given current config, performs work for that state,
    // returns the next state to transition to.
    using StateHandler = std::function<BotState(BotConfig&)>;

    // Lifecycle
    void Start();
    void Stop();
    bool IsRunning();

    // State machine
    BotState GetState();
    void SetState(BotState state);
    void RegisterStateHandler(BotState state, StateHandler handler);

    // Config
    BotConfig& GetConfig();
    void LoadConfigFromIni(const char* iniPath);

    // Logging
    void LogBot(const char* fmt, ...);

} // namespace GWA3::Bot

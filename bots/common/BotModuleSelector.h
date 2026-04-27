#pragma once

namespace GWA3::Bot {

enum class BotModuleKind {
    FroggyHM,
    RragarsMenagerie,
    Kathandrax,
    FrostmawsBurrows,
    RavensPoint,
    ArachnisHaunt,
};

BotModuleKind ParseBotModuleName(const char* name);
const char* GetBotModuleName(BotModuleKind kind);

} // namespace GWA3::Bot

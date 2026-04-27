#include <bots/common/BotModuleSelector.h>

#include <cstring>

namespace GWA3::Bot {

BotModuleKind ParseBotModuleName(const char* name) {
    if (name == nullptr || name[0] == '\0') {
        return BotModuleKind::FroggyHM;
    }
    if (_stricmp(name, "RragarsMenagerie") == 0 || _stricmp(name, "Rragar") == 0) {
        return BotModuleKind::RragarsMenagerie;
    }
    if (_stricmp(name, "Kathandrax") == 0 || _stricmp(name, "CatacombsOfKathandrax") == 0) {
        return BotModuleKind::Kathandrax;
    }
    if (_stricmp(name, "FrostmawsBurrows") == 0 || _stricmp(name, "Frostmaw") == 0) {
        return BotModuleKind::FrostmawsBurrows;
    }
    if (_stricmp(name, "RavensPoint") == 0 || _stricmp(name, "Ravens") == 0) {
        return BotModuleKind::RavensPoint;
    }
    if (_stricmp(name, "ArachnisHaunt") == 0 || _stricmp(name, "Arachnis") == 0) {
        return BotModuleKind::ArachnisHaunt;
    }
    return BotModuleKind::FroggyHM;
}

const char* GetBotModuleName(BotModuleKind kind) {
    switch (kind) {
    case BotModuleKind::RragarsMenagerie:
        return "RragarsMenagerie";
    case BotModuleKind::Kathandrax:
        return "Kathandrax";
    case BotModuleKind::FrostmawsBurrows:
        return "FrostmawsBurrows";
    case BotModuleKind::RavensPoint:
        return "RavensPoint";
    case BotModuleKind::ArachnisHaunt:
        return "ArachnisHaunt";
    case BotModuleKind::FroggyHM:
    default:
        return "FroggyHM";
    }
}

} // namespace GWA3::Bot

#include <gwa3/dungeon/DungeonOutpostSetup.h>

#include <gwa3/core/Log.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/managers/PlayerMgr.h>
#include <gwa3/managers/SkillMgr.h>

#include <Windows.h>

#include <array>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>

namespace GWA3::DungeonOutpostSetup {

namespace {

constexpr const char* kBase64Chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void WaitMs(DWORD ms) {
    Sleep(ms);
}

void NormalizeHeroConfigFilename(const char* input, char* output, std::size_t output_size) {
    if (!output || output_size == 0u) {
        return;
    }

    const char* source = (input && *input) ? input : "Standard.txt";
    const std::size_t source_len = std::strlen(source);
    if (source_len >= 4u && _stricmp(source + source_len - 4u, ".txt") == 0) {
        snprintf(output, output_size, "%s", source);
    } else {
        snprintf(output, output_size, "%s.txt", source);
    }
}

bool BuildRepoRelativePath(const char* relative_suffix, char* out_path, std::size_t out_path_size) {
    if (!relative_suffix || !*relative_suffix || !out_path || out_path_size == 0u) {
        return false;
    }

    char module_path[MAX_PATH] = {};
    HMODULE self = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(&BuildRepoRelativePath),
                            &self)) {
        return false;
    }
    if (!GetModuleFileNameA(self, module_path, MAX_PATH)) {
        return false;
    }

    char* slash = std::strrchr(module_path, '\\');
    if (slash) {
        *(slash + 1) = '\0';
    }

    constexpr const char* kPrefixes[] = {
        "",
        "..\\..\\..\\..\\",
        "..\\..\\..\\",
        "..\\..\\",
    };

    for (const char* prefix : kPrefixes) {
        std::snprintf(out_path, out_path_size, "%s%s%s", module_path, prefix, relative_suffix);
        if (GetFileAttributesA(out_path) != INVALID_FILE_ATTRIBUTES) {
            return true;
        }
    }

    std::snprintf(out_path, out_path_size, "%s%s%s", module_path, kPrefixes[0], relative_suffix);
    return false;
}

bool ReadTextFile(const char* path, std::string& out_text) {
    if (!path || !*path) {
        return false;
    }

    FILE* file = nullptr;
    fopen_s(&file, path, "rb");
    if (!file) {
        return false;
    }

    fseek(file, 0, SEEK_END);
    const long length = ftell(file);
    if (length <= 0) {
        fclose(file);
        return false;
    }
    fseek(file, 0, SEEK_SET);

    out_text.resize(static_cast<std::size_t>(length));
    const std::size_t read = fread(out_text.data(), 1, static_cast<std::size_t>(length), file);
    fclose(file);
    return read == static_cast<std::size_t>(length);
}

bool ReadIniHeroConfig(const char* ini_path,
                       const char* section,
                       const char* key,
                       char* out_filename,
                       std::size_t out_filename_size) {
    if (!ini_path || !*ini_path || !section || !*section || !key || !*key ||
        !out_filename || out_filename_size == 0u) {
        return false;
    }

    char value[128] = {};
    GetPrivateProfileStringA(section, key, "", value, sizeof(value), ini_path);
    if (value[0] == '\0') {
        return false;
    }

    NormalizeHeroConfigFilename(value, out_filename, out_filename_size);
    return true;
}

bool ResolvePreferredHeroConfigFromIni(const char* ini_path,
                                       const char* player_name,
                                       char* out_filename,
                                       std::size_t out_filename_size,
                                       const char* default_filename) {
    if (!out_filename || out_filename_size == 0u) {
        return false;
    }

    NormalizeHeroConfigFilename(default_filename, out_filename, out_filename_size);
    if (!ini_path || !*ini_path || GetFileAttributesA(ini_path) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }

    if (player_name && *player_name &&
        ReadIniHeroConfig(ini_path, "HeroConfigs", player_name, out_filename, out_filename_size)) {
        return true;
    }

    if (ReadIniHeroConfig(ini_path, "Settings", "HeroConfig", out_filename, out_filename_size)) {
        return true;
    }

    return false;
}

int Base64CharToVal(char c) {
    const char* p = std::strchr(kBase64Chars, c);
    return p ? static_cast<int>(p - kBase64Chars) : -1;
}

bool WaitForHeroCount(uint32_t target_count, DWORD timeout_ms, DWORD poll_interval_ms) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeout_ms) {
        if (PartyMgr::CountPartyHeroes() >= target_count) {
            return true;
        }
        WaitMs(poll_interval_ms);
    }
    return PartyMgr::CountPartyHeroes() >= target_count;
}

bool WaitForHeroesCleared(DWORD timeout_ms, DWORD poll_interval_ms) {
    const DWORD start = GetTickCount();
    bool reissued = false;
    while ((GetTickCount() - start) < timeout_ms) {
        if (PartyMgr::CountPartyHeroes() == 0u) {
            return true;
        }
        if (!reissued && (GetTickCount() - start) >= 2000u) {
            PartyMgr::KickAllHeroes();
            reissued = true;
        }
        WaitMs(poll_interval_ms);
    }
    return PartyMgr::CountPartyHeroes() == 0u;
}

} // namespace

bool DecodeSkillTemplate(const char* code, uint32_t skill_ids[8]) {
    if (!code || !*code || !skill_ids) {
        return false;
    }

    uint8_t bits[256] = {};
    int total_bits = 0;
    for (int i = 0; code[i] && total_bits < 240; ++i) {
        const int val = Base64CharToVal(code[i]);
        if (val < 0) {
            continue;
        }
        for (int bit = 0; bit < 6; ++bit) {
            bits[total_bits++] = static_cast<uint8_t>((val >> bit) & 1);
        }
    }

    int pos = 0;
    auto read_bits = [&](int count) -> uint32_t {
        uint32_t value = 0u;
        for (int i = 0; i < count && pos < total_bits; ++i) {
            value |= static_cast<uint32_t>(bits[pos++]) << i;
        }
        return value;
    };

    const uint32_t header = read_bits(4);
    if (header == 14u) {
        read_bits(4);
    } else if (header != 0u) {
        return false;
    }

    const uint32_t profession_bits = read_bits(2) * 2u + 4u;
    read_bits(static_cast<int>(profession_bits));
    read_bits(static_cast<int>(profession_bits));

    const uint32_t attribute_count = read_bits(4);
    const uint32_t attribute_bits = read_bits(4) + 4u;
    for (uint32_t i = 0; i < attribute_count; ++i) {
        read_bits(static_cast<int>(attribute_bits));
        read_bits(4);
    }

    const uint32_t skill_bits = read_bits(4) + 8u;
    for (int i = 0; i < 8; ++i) {
        skill_ids[i] = read_bits(static_cast<int>(skill_bits));
    }

    return true;
}

bool ResolvePreferredHeroConfigFromJson(const char* json_text,
                                        const char* player_name,
                                        char* out_filename,
                                        std::size_t out_filename_size) {
    if (!out_filename || out_filename_size == 0u) {
        return false;
    }

    NormalizeHeroConfigFilename("Standard.txt", out_filename, out_filename_size);
    if (!json_text || !*json_text || !player_name || !*player_name) {
        return true;
    }

    const std::string json(json_text);
    const std::string quoted_name = "\"" + std::string(player_name) + "\"";
    const std::size_t name_pos = json.find(quoted_name);
    if (name_pos == std::string::npos) {
        return true;
    }

    const std::size_t hero_config_pos = json.find("\"hero_config\"", name_pos);
    if (hero_config_pos == std::string::npos) {
        return true;
    }

    const std::size_t colon_pos = json.find(':', hero_config_pos);
    if (colon_pos == std::string::npos) {
        return true;
    }

    const std::size_t first_quote = json.find('"', colon_pos + 1u);
    if (first_quote == std::string::npos) {
        return true;
    }

    const std::size_t second_quote = json.find('"', first_quote + 1u);
    if (second_quote == std::string::npos || second_quote <= first_quote + 1u) {
        return true;
    }

    const std::string base_name = json.substr(first_quote + 1u, second_quote - first_quote - 1u);
    NormalizeHeroConfigFilename(base_name.c_str(), out_filename, out_filename_size);
    return true;
}

bool ResolvePreferredHeroConfigFile(char* out_filename,
                                    std::size_t out_filename_size,
                                    const char* default_filename) {
    if (!out_filename || out_filename_size == 0u) {
        return false;
    }

    NormalizeHeroConfigFilename(default_filename, out_filename, out_filename_size);

    wchar_t* player_name_w = PlayerMgr::GetPlayerName(0u);
    if (!player_name_w || !*player_name_w) {
        return true;
    }

    char player_name[128] = {};
    const int converted = WideCharToMultiByte(CP_ACP,
                                              0,
                                              player_name_w,
                                              -1,
                                              player_name,
                                              static_cast<int>(sizeof(player_name)),
                                              nullptr,
                                              nullptr);
    if (converted <= 1) {
        char public_config_path[MAX_PATH] = {};
        BuildRepoRelativePath("config\\gwa3.ini", public_config_path, sizeof(public_config_path));
        ResolvePreferredHeroConfigFromIni(public_config_path,
                                          nullptr,
                                          out_filename,
                                          out_filename_size,
                                          default_filename);
        return true;
    }

    char public_config_path[MAX_PATH] = {};
    BuildRepoRelativePath("config\\gwa3.ini", public_config_path, sizeof(public_config_path));
    if (ResolvePreferredHeroConfigFromIni(public_config_path,
                                          player_name,
                                          out_filename,
                                          out_filename_size,
                                          default_filename)) {
        return true;
    }

    char config_path[MAX_PATH] = {};
    BuildRepoRelativePath("GWA Censured\\AccountConfigs.json", config_path, sizeof(config_path));

    std::string json;
    if (!ReadTextFile(config_path, json)) {
        return true;
    }

    return ResolvePreferredHeroConfigFromJson(json.c_str(), player_name, out_filename, out_filename_size);
}

std::size_t LoadHeroTemplatesFromFile(const char* filename,
                                      HeroTemplate* out_templates,
                                      std::size_t max_count) {
    if (!filename || !*filename || !out_templates || max_count == 0u) {
        return 0u;
    }

    char normalized_name[64] = {};
    NormalizeHeroConfigFilename(filename, normalized_name, sizeof(normalized_name));

    FILE* file = nullptr;
    constexpr const char* kHeroConfigDirs[] = {
        "config\\hero_configs",
        "GWA Censured\\hero_configs",
    };

    for (const char* dir : kHeroConfigDirs) {
        char path[MAX_PATH] = {};
        BuildRepoRelativePath(dir, path, sizeof(path));
        const std::size_t used = std::strlen(path);
        if (used + 1u + std::strlen(normalized_name) >= sizeof(path)) {
            continue;
        }
        if (used > 0u && path[used - 1u] != '\\') {
            strcat_s(path, "\\");
        }
        strcat_s(path, normalized_name);

        fopen_s(&file, path, "r");
        if (file) {
            break;
        }
    }

    if (!file) {
        return 0u;
    }

    std::size_t count = 0u;
    char line[512] = {};
    while (count < max_count && std::fgets(line, sizeof(line), file)) {
        char* semi = std::strchr(line, ';');
        if (semi) {
            *semi = '\0';
        }

        char* p = line;
        while (*p == ' ' || *p == '\t') {
            ++p;
        }
        if (*p == '\0' || *p == '\n' || *p == '\r') {
            continue;
        }

        char* comma = std::strchr(p, ',');
        if (!comma) {
            continue;
        }
        *comma = '\0';

        const uint32_t hero_id = static_cast<uint32_t>(std::atoi(p));
        char* template_code = comma + 1;
        while (*template_code == ' ' || *template_code == '\t') {
            ++template_code;
        }

        char* end = template_code + std::strlen(template_code);
        while (end > template_code &&
               (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t')) {
            *--end = '\0';
        }

        if (hero_id == 0u || *template_code == '\0') {
            continue;
        }

        HeroTemplate& hero = out_templates[count];
        hero = {};
        hero.hero_id = hero_id;
        if (!DecodeSkillTemplate(template_code, hero.skills)) {
            continue;
        }

        ++count;
    }

    fclose(file);
    return count;
}

bool ApplyOutpostSetup(Config& cfg, const Options& options) {
    char hero_config_file[64] = {};
    if (!cfg.hero_config_file.empty()) {
        NormalizeHeroConfigFilename(cfg.hero_config_file.c_str(), hero_config_file, sizeof(hero_config_file));
    } else {
        ResolvePreferredHeroConfigFile(hero_config_file,
                                       sizeof(hero_config_file),
                                       options.default_hero_config_file);
    }
    cfg.hero_config_file = hero_config_file;
    GWA3::Log::Info("Dungeon outpost setup using hero config: %s", cfg.hero_config_file.c_str());

    std::array<uint32_t, kMaxHeroTemplates> fallback_hero_ids = {};
    for (std::size_t i = 0; i < fallback_hero_ids.size(); ++i) {
        fallback_hero_ids[i] = cfg.hero_ids[i];
    }

    if (PartyMgr::CountPartyHeroes() > 0u) {
        GWA3::Log::Info("Clearing %u existing heroes before dungeon setup", PartyMgr::CountPartyHeroes());
        PartyMgr::KickAllHeroes();
        if (!WaitForHeroesCleared(options.clear_timeout_ms, options.clear_poll_interval_ms)) {
            GWA3::Log::Info("Dungeon outpost setup failed: heroes did not clear");
            return false;
        }
    }

    for (uint32_t& hero_id : cfg.hero_ids) {
        hero_id = 0u;
    }

    std::array<HeroTemplate, kMaxHeroTemplates> templates = {};
    const std::size_t template_count = LoadHeroTemplatesFromFile(cfg.hero_config_file.c_str(),
                                                                 templates.data(),
                                                                 templates.size());

    uint32_t active_hero_count = 0u;
    if (template_count > 0u) {
        for (std::size_t i = 0; i < template_count; ++i) {
            cfg.hero_ids[i] = templates[i].hero_id;
            PartyMgr::AddHero(templates[i].hero_id);
            if (!WaitForHeroCount(static_cast<uint32_t>(i + 1u),
                                  options.add_hero_timeout_ms,
                                  options.clear_poll_interval_ms)) {
                GWA3::Log::Info("Dungeon outpost setup failed: hero %u did not join slot %zu",
                       templates[i].hero_id,
                       i + 1u);
                return false;
            }
            WaitMs(options.add_hero_delay_ms);
        }

        active_hero_count = static_cast<uint32_t>(template_count);

        for (uint32_t hero_index = 1u; hero_index <= active_hero_count; ++hero_index) {
            SkillMgr::LoadSkillbar(templates[hero_index - 1u].skills, hero_index);
            WaitMs(options.skillbar_delay_ms);
        }
    } else {
        GWA3::Log::Info("Dungeon outpost setup falling back to preconfigured hero ids");
        for (std::size_t i = 0; i < fallback_hero_ids.size(); ++i) {
            const uint32_t hero_id = fallback_hero_ids[i];
            if (hero_id == 0u) {
                continue;
            }
            cfg.hero_ids[i] = hero_id;
            PartyMgr::AddHero(hero_id);
            if (!WaitForHeroCount(active_hero_count + 1u,
                                  options.add_hero_timeout_ms,
                                  options.clear_poll_interval_ms)) {
                GWA3::Log::Info("Dungeon outpost setup failed: fallback hero %u did not join", hero_id);
                return false;
            }
            ++active_hero_count;
            WaitMs(options.add_hero_delay_ms);
        }
    }

    if (active_hero_count == 0u) {
        GWA3::Log::Info("Dungeon outpost setup failed: no heroes configured");
        return false;
    }

    if (options.enable_hard_mode && cfg.hard_mode) {
        MapMgr::SetHardMode(true);
        WaitMs(options.hard_mode_delay_ms);
    }

    for (uint32_t hero_index = 1u; hero_index <= active_hero_count; ++hero_index) {
        PartyMgr::SetHeroBehavior(hero_index, options.hero_behavior);
        WaitMs(options.hero_behavior_delay_ms);
    }

    GWA3::Log::Info("Dungeon outpost setup complete: heroes=%u hardMode=%u",
           active_hero_count,
           (cfg.hard_mode && options.enable_hard_mode) ? 1u : 0u);
    return true;
}

} // namespace GWA3::DungeonOutpostSetup

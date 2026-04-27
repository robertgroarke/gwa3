#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace GWA3::DungeonOutpostSetup {

inline constexpr std::size_t kMaxHeroTemplates = 7u;

struct HeroTemplate {
    uint32_t hero_id = 0u;
    uint32_t skills[8] = {};
};

struct Options {
    const char* default_hero_config_file = "Standard.txt";
    uint32_t hero_behavior = 1u;
    uint32_t clear_timeout_ms = 5000u;
    uint32_t clear_poll_interval_ms = 250u;
    uint32_t add_hero_timeout_ms = 5000u;
    uint32_t add_hero_delay_ms = 300u;
    uint32_t skillbar_delay_ms = 500u;
    uint32_t hard_mode_delay_ms = 500u;
    uint32_t hero_behavior_delay_ms = 100u;
    bool enable_hard_mode = true;
};

struct Config {
    bool hard_mode = true;
    uint32_t hero_ids[kMaxHeroTemplates] = {};
    std::string hero_config_file;
};

bool DecodeSkillTemplate(const char* code, uint32_t skill_ids[8]);

bool ResolvePreferredHeroConfigFromJson(const char* json_text,
                                        const char* player_name,
                                        char* out_filename,
                                        std::size_t out_filename_size);

bool ResolvePreferredHeroConfigFile(char* out_filename,
                                    std::size_t out_filename_size,
                                    const char* default_filename = "Standard.txt");

std::size_t LoadHeroTemplatesFromFile(const char* filename,
                                      HeroTemplate* out_templates,
                                      std::size_t max_count);

bool ApplyOutpostSetup(Config& cfg, const Options& options = {});

} // namespace GWA3::DungeonOutpostSetup

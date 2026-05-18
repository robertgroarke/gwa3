// Bogroot Growths Level 1 blessing shrine coordinates (from AutoIt waypoint "Blessing Lvl1").
static constexpr float kBlessingX = 19099.0f;
static constexpr float kBlessingY = 7762.0f;

// Title display IDs for SetActiveTitle packet (0x58).
// These match GWCA TitleID enum, NOT the title track array index.
// AutoIt: $ID_ASURA_TITLE=0x26, $ID_DWARF_TITLE=0x27, $ID_EBON_VANGUARD_TITLE=0x28, $ID_NORN_TITLE=0x29
static constexpr uint32_t TITLE_DISPLAY_ASURA     = 0x26; // 38
static constexpr uint32_t TITLE_DISPLAY_DELDRIMOR = Bot::Froggy::BLESSING_TITLE_ID; // 39
static constexpr uint32_t TITLE_DISPLAY_VANGUARD  = 0x28; // 40
static constexpr uint32_t TITLE_DISPLAY_NORN      = 0x29; // 41

static constexpr uint32_t DIALOG_ACCEPT_BLESSING = Bot::Froggy::BLESSING_ACCEPT_DIALOG_ID;

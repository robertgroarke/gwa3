struct MoveStep {
    float x;
    float y;
    float threshold;
    int timeoutMs;
    const char* label;
};

static bool KickAllHeroesWithObservation(DWORD timeoutMs);

static const MoveStep kSparkflyToTekksPath[] = {
    {-4559.0f, -14406.0f, 500.0f, 25000, "Sparkfly waypoint 1"},
    {-5204.0f, -9831.0f,  500.0f, 25000, "Sparkfly waypoint 2"},
    {-928.0f,  -8699.0f,  500.0f, 25000, "Sparkfly waypoint 3"},
    {4200.0f,  -4897.0f,  500.0f, 25000, "Sparkfly waypoint 4"},
    {6114.0f,  819.0f,    500.0f, 25000, "Sparkfly waypoint 5"},
    {9500.0f,  2281.0f,   500.0f, 25000, "Sparkfly waypoint 6"},
    {11570.0f, 6120.0f,   500.0f, 25000, "Sparkfly waypoint 7"},
    {11025.0f, 11710.0f,  500.0f, 25000, "Sparkfly waypoint 8"},
    {14624.0f, 19314.0f,  500.0f, 30000, "Sparkfly waypoint 9"},
    {kTekksX,  kTekksY,   250.0f, 25000, "Tekks"},
};

// From Tekks to Bogroot dungeon entrance (mirrors AutoIt TakeQuest0 post-quest path).
static const MoveStep kTekksToDungeonPath[] = {
    {12228.0f, 22677.0f, 500.0f, 15000, "Dungeon approach 1"},
    {12470.0f, 25036.0f, 500.0f, 15000, "Dungeon approach 2"},
    {12968.0f, 26219.0f, 500.0f, 15000, "Dungeon approach 3"},
};
static constexpr float kDungeonPortalX = 13097.0f;
static constexpr float kDungeonPortalY = 26393.0f;

// First waypoints inside Bogroot Lvl1, from spawn to blessing shrine.
static const MoveStep kBogrootToBlessingPath[] = {
    {17026.0f, 2168.0f,  500.0f, 25000, "Bogroot start"},
    {kBlessingX, kBlessingY, 500.0f, 30000, "Blessing shrine"},
};

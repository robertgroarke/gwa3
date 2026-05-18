static bool HasAnyBlessing() {
    uint32_t myId = AgentMgr::GetMyId();
    if (myId == 0) return false;
    return GWA3::DungeonEffects::HasAnyDungeonBlessing(myId);
}

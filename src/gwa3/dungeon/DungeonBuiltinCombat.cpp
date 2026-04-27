#include <gwa3/dungeon/DungeonBuiltinCombat.h>

#include <gwa3/dungeon/DungeonCombatRoutine.h>
#include <gwa3/dungeon/DungeonLoot.h>
#include <gwa3/dungeon/DungeonSkill.h>
#include <gwa3/managers/AgentMgr.h>
#include <gwa3/managers/MapMgr.h>
#include <gwa3/managers/PartyMgr.h>
#include <gwa3/packets/CtoS.h>

#include <Windows.h>

namespace GWA3::DungeonBuiltinCombat {

namespace {

bool WaitForCombatSettle(uint32_t timeoutMs = 1000u) {
    const DWORD start = GetTickCount();
    while ((GetTickCount() - start) < timeoutMs) {
        auto* me = AgentMgr::GetMyAgent();
        if (!me) {
            return false;
        }
        if (me->move_x == 0.0f &&
            me->move_y == 0.0f &&
            GWA3::CtoS::IsBotshubQueueIdle()) {
            return true;
        }
        Sleep(50u);
    }
    return false;
}

bool CanBasicAttack() {
    DungeonSkill::CachedSkill basicAttack = {};
    basicAttack.slot = 0xFFu;
    basicAttack.skill_type = 9u;
    return DungeonSkill::CanCast(basicAttack);
}

int PickUpNearbyLoot(float maxRange) {
    DungeonLoot::LootPickupOptions options;
    options.interact_threshold = 200.0f;
    options.move_timeout_ms = 5000u;
    options.move_poll_ms = 100u;
    options.pickup_delay_ms = 250u;
    return DungeonLoot::PickUpNearbyLoot(
        maxRange,
        &GWA3::DungeonBuiltinCombat::WaitMs,
        &GWA3::DungeonBuiltinCombat::IsPlayerOrPartyDead,
        options);
}

void SettleAfterLocalClear(uint32_t timeoutMs = 1500u) {
        // Stop active combat before resuming travel so route movement can regain control.
    // here so route movement does not overlap stale attack/chase commands.
    AgentMgr::CancelAction();
    WaitMs(100u);
    (void)WaitForCombatSettle(timeoutMs);
}

} // namespace

void WaitMs(uint32_t ms) {
    Sleep(ms);
}

bool IsPlayerOrPartyDead() {
    auto* me = AgentMgr::GetMyAgent();
    return me == nullptr || me->hp <= 0.0f || PartyMgr::GetIsPartyDefeated();
}

bool IsCurrentMapLoaded() {
    return MapMgr::GetMapId() != 0u && MapMgr::GetLoadingState() == 1u;
}

void QueueAggroMove(float x, float y) {
    AgentMgr::Move(x, y);
}

void AutoAttackTarget(uint32_t targetId) {
    AgentMgr::Attack(targetId);
}

void ConfigureBuiltinAggroAdvanceOptions(
    DungeonCombat::AggroAdvanceOptions& options,
    uint32_t timeoutMs,
    bool pickupAfterClear) {
    options.timeout_ms = timeoutMs;
    options.clear_options.pickup_after_clear = pickupAfterClear;
    options.clear_options.flag_heroes = false;
    options.clear_options.change_target = false;
    options.clear_options.call_target = false;
    options.clear_options.chase_during_clear = false;
    options.clear_options.hold_movement_for_local_clear = true;
    options.clear_options.quiet_confirmation_ms = 1250u;
    options.clear_options.chase_wait_ms = 350u;
    options.clear_options.pre_clear_cancel_wait_ms = 50u;
    options.clear_options.post_clear_cancel_wait_ms = 150u;
    options.clear_options.idle_wait_ms = 150u;
    options.clear_options.loop_wait_ms = 250u;
    options.clear_options.fight_reissue_ms = 750u;
    options.clear_options.attack_reissue_ms = 750u;
    options.clear_options.timeout_ms = timeoutMs;
    options.clear_options.target_timeout_ms = timeoutMs;
}

void FightTargetWithBuiltinCombat(uint32_t targetId) {
    if (targetId == 0u) {
        return;
    }

    auto* me = AgentMgr::GetMyAgent();
    if (!me || AgentMgr::IsCasting(me)) {
        return;
    }

    DungeonSkill::CachedSkill skillCache[8] = {};
    bool usedThisStep[8] = {};
    if (!DungeonSkill::BuildSkillCache(skillCache)) {
        return;
    }

    DungeonCombatRoutine::SkillExecutionContext context;
    context.skill_cache = skillCache;
    context.skill_used_this_step = usedThisStep;
    context.skill_count = 8u;
    context.wait_ms = &WaitMs;
    context.is_dead = &IsPlayerOrPartyDead;

    DungeonCombatRoutine::SkillActionResult action = {};
    (void)DungeonCombatRoutine::UseSkillsInSlotOrder(targetId, context, &action);
}

void FightTargetWithPriorityBuiltinCombat(uint32_t targetId) {
    if (targetId == 0u) {
        return;
    }

    auto* me = AgentMgr::GetMyAgent();
    if (!me || AgentMgr::IsCasting(me)) {
        return;
    }

    if (CanBasicAttack()) {
        AutoAttackTarget(targetId);
        WaitMs(100u);
    }

    DungeonSkill::CachedSkill skillCache[8] = {};
    bool usedThisStep[8] = {};
    if (!DungeonSkill::BuildSkillCache(skillCache)) {
        return;
    }

    DungeonCombatRoutine::SkillExecutionContext context;
    context.skill_cache = skillCache;
    context.skill_used_this_step = usedThisStep;
    context.skill_count = 8u;
    context.wait_ms = &WaitMs;
    context.is_dead = &IsPlayerOrPartyDead;

    DungeonCombatRoutine::SkillActionResult action = {};
    (void)DungeonCombatRoutine::ExecuteCombatStep(targetId, context, nullptr, action);
}

DungeonCombat::CombatCallbacks MakeCombatCallbacks() {
    DungeonCombat::CombatCallbacks callbacks;
    callbacks.is_dead = &IsPlayerOrPartyDead;
    callbacks.is_map_loaded = &IsCurrentMapLoaded;
    callbacks.wait_ms = &WaitMs;
    callbacks.queue_move = &QueueAggroMove;
    callbacks.fight_target = &FightTargetWithBuiltinCombat;
    callbacks.pickup_loot = &PickUpNearbyLoot;
    return callbacks;
}

bool MoveToPointWithAggro(
    float x,
    float y,
    uint32_t mapId,
    float tolerance,
    float fightRange,
    uint32_t timeoutMs) {
    if (MapMgr::GetMapId() != mapId || !MapMgr::GetIsMapLoaded()) {
        return false;
    }

    auto* me = AgentMgr::GetMyAgent();
    if (me && AgentMgr::GetDistance(me->x, me->y, x, y) <= tolerance) {
        return true;
    }

    DungeonCombat::AggroAdvanceOptions options;
    options.arrival_threshold = tolerance;
    options.move_wait_ms = 100u;
    ConfigureBuiltinAggroAdvanceOptions(options, timeoutMs, true);
    const bool advanced = DungeonCombat::AdvanceWithAggro(
        x,
        y,
        fightRange,
        MakeCombatCallbacks(),
        options);
    if (advanced) {
        SettleAfterLocalClear();
    }
    return advanced;
}

bool FollowTravelPathWithAggro(
    const DungeonQuest::TravelPoint* points,
    int count,
    uint32_t mapId,
    float tolerance,
    float fightRange,
    uint32_t timeoutMs) {
    if (points == nullptr || count <= 0) {
        return false;
    }

    for (int i = 0; i < count; ++i) {
        if (!MoveToPointWithAggro(
                points[i].x,
                points[i].y,
                mapId,
                tolerance,
                fightRange,
                timeoutMs)) {
            return false;
        }
    }

    return true;
}

} // namespace GWA3::DungeonBuiltinCombat

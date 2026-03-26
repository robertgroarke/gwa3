# Froggy Smoke Test Checklist

Run this after each merged extraction phase. Tests require a running Guild Wars client.

## Pre-flight
- [ ] Au3Check passes (`bash tests/run_au3check.sh` — errors ≤ 191)
- [ ] Keystone ASM suite passes (`python tests/test_asm_keystone.py`)

## Launch
- [ ] Script starts without crash (GUI appears)
- [ ] "Start" button initiates client connection
- [ ] Character name detected and displayed
- [ ] Hero setup loads (if "Add Heroes" checked)

## Travel
- [ ] TravelTo successfully zones to Gadds/EotN
- [ ] DP consumable usage works (if applicable)
- [ ] District selection works

## Route / Waypoints
- [ ] Enters explorable area (Bogroot Growths)
- [ ] Follows waypoint path without getting stuck
- [ ] Aggro movement engages enemies correctly

## Combat
- [ ] Skills fire on correct targets
- [ ] Skill classification (heal, hex, condition) routes to right targets
- [ ] Hero skills used appropriately
- [ ] Wipe detection triggers correctly
- [ ] Wipe recovery returns to outpost

## Looting
- [ ] Items picked up after combat
- [ ] Chest detection and opening works
- [ ] Rare skin protection (not sold/salvaged)
- [ ] Pickup filters work (golds, lockpicks, dyes)

## Maintenance
- [ ] RunDiagnostics detects low inventory/kits
- [ ] PerformMaintenance travels to town
- [ ] ID kits purchased
- [ ] Salvage kits purchased
- [ ] Items identified
- [ ] Items salvaged (mod extraction works)
- [ ] Materials sold
- [ ] Gold deposited

## Stats
- [ ] Run counter increments
- [ ] Fail counter tracks wipes
- [ ] Run time displayed
- [ ] Drop statistics update

## Recovery
- [ ] Disconnection recovery reconnects
- [ ] Script continues after reconnect
- [ ] Multiple consecutive runs complete

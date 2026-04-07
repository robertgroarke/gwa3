# GWCA Live Detour Classification Addendum

## Scope

This pass takes the live hook-table dump and asks the next practical question:

- which entries can we confidently assign to specific GWCA subsystems?

The answer is not "all of them" yet, but we can now classify a meaningful subset with direct evidence instead of guesswork.

## Strongest confirmed cluster: UIModule

The compiled UIModule bootstrap at:

- [FUN_10024690](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_10024690.log)

creates these six hooks:

- `DAT_1008a360` -> detour `FUN_10026800`
- `DAT_1008a368` -> detour `FUN_100268A0`
- `DAT_1008a384` -> detour `FUN_10026470`
- `DAT_1008a39C` -> detour `FUN_10026860`
- `DAT_1008a3A8` -> detour `FUN_100265C0`
- `DAT_1008a370` -> detour `FUN_100268C0`

The compiled UIModule enabler at:

- [FUN_10024E80](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_10024e80.log)

then enables exactly those six hook targets.

That means any live hook-table entry whose detour RVA is one of:

- `0x26800`
- `0x268A0`
- `0x26470`
- `0x26860`
- `0x265C0`
- `0x268C0`

belongs to the UIModule cluster.

## Live UIModule entries from the current client

From:

- [live_gwca_runtime_report.txt](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\tests\live_gwca_runtime_report.txt)

the confirmed UIModule entries are:

### Entry 5

- detour RVA `0x26800`
- target `0x00FAACE0`
- replay `0x0A110F40`
- classification: UIModule / Win32-input activity hook

### Entry 6

- detour RVA `0x268A0`
- target `0x00FF02C0`
- replay `0x0A110F20`
- classification: UIModule / global `SendUIMessage` shim

### Entry 7

- detour RVA `0x26470`
- target `0x00FED300`
- replay `0x0A110F00`
- classification: UIModule / named compiled UI detour at `FUN_10026470`

### Entry 8

- detour RVA `0x26860`
- target `0x010086D0`
- replay `0x0A110EE0`
- classification: UIModule / frame `SendFrameUIMsg` shim

### Entry 9

- detour RVA `0x265C0`
- target `0x012CB250`
- replay `0x0A110EC0`
- classification: UIModule / compiled passthrough detour at `FUN_100265C0`

### Entry 10

- detour RVA `0x268C0`
- target `0x0100ABB0`
- replay `0x0A110EA0`
- classification: UIModule / compiled UI detour at `FUN_100268C0`

That gives us one fully confirmed subsystem slice:

- 6 live entries
- 6 known detour RVAs
- 6 hooks statically created and enabled by UIModule

## Confirmed example outside UI: Render module

We also have a compiled module-level example for Render:

- [GW::Render::EnableHooks](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_10020840_deep.log)

which enables:

- `DAT_1008a26C`
- `DAT_1008a298`

This confirms an important architectural pattern:

- modules own hook-target globals
- module `EnableHooks()` forwards those targets into the shared hook engine

What we do **not** yet have from this pass is the live target/detour mapping for those two Render hooks. So Render is source/binary-proven as a detour-using subsystem, but not yet live-table-resolved in the same way UIModule is.

## What remains unresolved in the live table

Most of the remaining detour RVAs are still unnamed in the current notes, for example:

- `0x2BC0`
- `0x2C00`
- `0x2CC0`
- `0x4720`
- `0x4760`
- `0x47C0`
- `0x4810`
- `0x4860`
- `0x48C0`
- `0x4900`
- `0x4940`
- `0x4970`
- `0x49B0`
- `0x183B0`
- `0x183D0`
- `0x1BDA0`
- `0x1BEB0`
- `0x1BFB0`
- `0x1C060`
- `0x1C220`
- `0x1C2C0`
- `0x1D8C0`
- `0x1E310`
- `0x1E3C0`
- `0x1F270`
- `0x1F2A0`
- `0x204D0`
- `0x20540`
- `0x20C00`
- `0x20C40`
- `0x20D90`
- `0x23180`

So this pass is not a full subsystem map yet.

## Important trust distinction

After this pass, we can sort the live table into three confidence tiers:

### Tier 1: fully resolved

- entries `5` through `10`
- live target, live replay slot, live detour RVA
- static creator and enabler both known
- subsystem confidently assigned to UIModule

### Tier 2: subsystem known, live entry not yet singled out

- Render module
- proven to use detour hooks via its module `EnableHooks()`
- but its corresponding live entries are not yet isolated from the table

### Tier 3: live entries present, detour RVA still unnamed

- the remainder of the table
- real live hooks
- but not yet attached to a named module/helper in the current research

## Why this matters for the replacement-feasibility question

This classification pass supports a more concrete engineering takeaway:

- some GWCA behavior, especially in the UI path, is now tied to a small, specific, live-confirmed detour set
- replacing or re-implementing GWCA does not mean replacing one giant black box at once
- at least one subsystem, UIModule, is already decomposed to a hook-by-hook level

That makes targeted replacement work more realistic:

- frame dispatcher hook
- global UI dispatcher hook
- adjacent UI pass-through hooks
- Win32 input activity hook

are no longer abstract dependencies

## Best next step

The next logical reverse pass is:

1. name more of the unresolved detour RVAs by matching them to nearby function logs and module `EnableHooks()` methods
2. extend the runtime dump to include memory-patcher records
3. build a single detour-vs-memory-patch classification matrix by subsystem

At this point, though, the UIModule hook cluster is fully live-classified.

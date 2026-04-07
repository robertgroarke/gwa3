# GWCA Remaining Decompilation / Disassembly Roadmap

This note outlines the remaining plausible reverse-engineering steps for the current GWCA work.

It is meant to turn the scattered "best next step" tails across the research notes into one practical backlog.

## Current Boundary

The work now splits into two real buckets:

1. `gwca.dll` / local helper / hook / scanner / wrapper analysis that we can continue immediately with the current workspace
2. game-side target analysis that needs the matching Guild Wars executable image loaded into Ghidra

That boundary matters because some of the highest-value next questions are no longer blocked by reasoning; they are blocked by artifact availability.

## Highest-Priority Remaining Steps

These are the strongest next moves if the goal is to keep making the most architectural progress per pass.

### 1. Finish the lower-family transfer-bank map

Why this is next:

- the transfer-bank branch is now highly structured
- `FUN_006848D0` exposed the lower family-`0` lane
- the remaining uncertainty is narrow and testable

Best next targets:

- find more callers of:
  - `FUN_006A5CB0(..., 0, *)`
  - `FUN_006A5CB0(..., 1, *)`
- add one appendix that maps:
  - `block_kind`
  - helper pair
  - observed modes
  - known caller set
  - worker subtype
- inspect any remaining upper-region outliers beyond:
  - `FUN_0068C310`

Questions this should answer:

- is lower family `0` a small special lane or a broader parallel branch?
- what are the exact semantics of intermediate modes `0/1/2/3`?
- how fully do the intermediate families map back to the upper DXT-derived families?

Primary notes to continue from:

- [GWCA_GameThread_TransferBank_Crosswalk.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_TransferBank_Crosswalk.md)
- [GWCA_GameThread_RepackerOnlyAdapterTaxonomyAppendix.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_RepackerOnlyAdapterTaxonomyAppendix.md)
- [GWCA_GameThread_Family0Family1EmitterSplit_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_Family0Family1EmitterSplit_Addendum.md)

### 2. Resolve the internal message / dispatch plane

Why this is next:

- the callback shell is no longer the main mystery
- the remaining unknowns sit in the internal message ids and dispatch fan-out

Best next targets:

- `FUN_006286D0`
- `FUN_00628A10`
- `FUN_00629A00`
- `FUN_006298F0`

Questions this should answer:

- what do message ids `0x24`, `0x2d`, and `0x2e` actually mean?
- is `FUN_00628A10(...)` a typed event bus, a phase router, or a phase-specific submit path?
- are the coordinate transforms viewport-space, client-space, or another frame-space convention?

Primary note to continue from:

- [GWCA_GameThread_MessagePath_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_MessagePath_Addendum.md)

### 3. Resolve typed phase lookup and backing objects

Why this is next:

- the global relation-message plane and owner-local phase plane are now separated
- the missing part is the actual typed phase object family behind the ids

Best next targets:

- `FUN_0062D5C0`
- more callers of `FUN_00628A10(...)`
- more callers of `FUN_00624700(...)`
- optionally more callers of `FUN_006287D0(...)`

Questions this should answer:

- which typed phases exist beyond `3` and `4`?
- what backing object family does each phase id resolve?
- where is the line between relation, selection, layout, and activation phases?

Primary note to continue from:

- [GWCA_GameThread_PhaseTaxonomy_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_PhaseTaxonomy_Addendum.md)

## Strong Secondary Steps

These are still good reverse targets, but they are slightly less central than the three items above.

### 4. Tighten hook-record / trampoline record layout

Best next targets:

1. identify the layout returned by `FUN_10029D60()`
2. characterize how `SendFrameUIMessage(...)` invokes that record as a continuation target
3. repeat the live-byte dump around the `+0x8A3A0` handle with a validated live-process probe

Questions this should answer:

- what is the exact layout of GWCA's allocated hook replay handle?
- what is callable code vs metadata inside the record?
- how much of the hook engine can be reconstructed as a stable reusable installer model?

Primary note to continue from:

- [GWCA_HookInstaller_DeepDive.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_HookInstaller_DeepDive.md)

### 5. Continue channel / extended protocol closure

Why this still matters:

- several channel and extended-cluster notes now point to tightening, not broad discovery
- this is a good medium-depth seam if we want progress outside transfer-bank work

Likely next targets:

- remaining active channel ids from the channel-family notes
- secondary objects mentioned in the extended-protocol notes
- unresolved executor / wrapper identities around the dispatch side

Primary notes to continue from:

- [GWCA_GameThread_ChannelClassification_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_ChannelClassification_Addendum.md)
- [GWCA_GameThread_ExtendedProtocol_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_ExtendedProtocol_Addendum.md)

### 6. Continue owner / relation / region helper naming

Why this remains useful:

- several structural addenda have already recovered the big planes
- what's left is mostly naming the remaining inner helpers and managers

Likely next targets:

- remaining owner-local emit helpers
- relation-owner validators
- region-table and layout helpers beneath:
  - `FUN_005F00F0(...)`

Primary notes to continue from:

- [GWCA_GameThread_OwnerRelationCluster_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_OwnerRelationCluster_Addendum.md)
- [GWCA_GameThread_ResetAndRegionTable_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_ResetAndRegionTable_Addendum.md)
- [GWCA_GameThread_RegionLayoutModes_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_RegionLayoutModes_Addendum.md)

## Artifact-Blocked Steps

These are real next steps, but they need the matching game executable image in the workspace.

### 7. Decompile the game-side raw frame dispatcher target

What is already known:

- GWCA recovers the raw frame dispatcher target via scan logic
- GWCA also anchors a nearby helper using the assertion:
  - `"hdr.reserved < frame->msg.m_classes.Count()"`

What is still missing:

- the actual game function bodies

Required artifact:

- the matching Guild Wars executable image for this GWCA build, imported into Ghidra

Once available, the next exact targets are:

- the game-side target behind `DAT_1008A39C`
- the `FrMsg.cpp` helper behind `DAT_1008A34C`

Questions this should answer:

- how are frame message classes routed at the game level?
- where is the class/opcode dispatch table?
- how closely does GWCA's UI hook map to the native game message system?

Primary note to continue from:

- [GWCA_UIMessage_Research.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\research\GWCA_Disassembly_Research\GWCA_UIMessage_Research.md)

## Lower-Priority Optional Sweeps

These are plausible sweeps if we want breadth rather than depth.

### 8. Sweep remaining outlier callers in already-mapped pockets

Use this when:

- we want confidence that no hidden sibling family sits just outside the sampled set

Best candidates:

- remaining `0x0068*` callers of `FUN_006A5F10(...)` / `FUN_006A5CB0(...)`
- remaining unresolved `FUN_00689E90(...)` consumers from the format-capability notes
- remaining unresolved callers around setup-created callbacks or piece-effect helpers

This is best used as:

- a closure pass after a core semantic branch is tightened,
- not as the default next move.

## Suggested Execution Order

If the goal is maximum value with the current workspace, the strongest order is:

1. finish the lower-family transfer-bank map
2. resolve the internal message / dispatch plane
3. resolve typed phase lookup and backing objects
4. tighten hook-record / trampoline record layout
5. continue channel / extended protocol closure
6. continue owner / relation / region helper naming
7. only after that, do broader outlier sweeps

If the matching game executable becomes available, insert this immediately after step 4:

1. decompile the raw frame dispatcher target
2. decompile the `FrMsg.cpp` helper
3. then fold those results back into the UI message and hook notes

## Practical Rule Of Thumb

When choosing the next decompilation target:

- prefer functions that answer an existing taxonomy gap
- prefer dispatchers, lookup helpers, and emitters over another sibling worker unless the sibling tests a branch boundary
- only widen to new pockets when the current pocket has stopped producing new semantic distinctions

That rule matches the recent high-yield passes and should keep the next wave of GWCA reversing efficient.

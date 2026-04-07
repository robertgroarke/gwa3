# GWCA Build Consistency And Export Seam Addendum

## Scope

This pass started as a continuation of the unnamed-patch-owner thread, but it uncovered a more important prerequisite:

- make sure the absolute data RVAs we are naming actually belong to the same `gwca.dll` build that the live client is loading

The injected build in the live tooling is:

- [toolbox/GWToolboxpp-master/Dependencies/GWCA/bin/gwca.dll](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\toolbox\GWToolboxpp-master\Dependencies\GWCA\bin\gwca.dll)

That is the path used by:

- [live_gwca_runtime_dump.ps1](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\scripts\live_gwca_runtime_dump.ps1)
- [test_gwca_inject_research.au3](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\debug_scripts\test_gwca_inject_research.au3)
- [GWA2_FrameUI.au3](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\lib\custom\GWA2_FrameUI.au3)

## Important correction: the absolute `0x10089F18` / `0x10089FA4` / `0x1008A188` anchors are not stable

When I re-checked the actual injected toolbox build directly, the bytes at:

- `0x10089F18`
- `0x10089FA4`
- `0x1008A188`

do **not** look like `MemoryPatcher` objects at all.

Instead, they land inside non-code data with UTF-16 / scalar-table looking content.

That means the previous ownership pass that treated:

- `DAT_10089f18`
- `DAT_10089fa4`
- `DAT_1008a188`

as patch-object globals should be treated as **build-fragile** rather than final.

The live memory-patcher dump is still real.
What changed is the confidence of the static absolute-global mapping inside the toolbox build we are actually injecting.

So the safest next step is not to keep forcing those globals.
It is to pivot to a better-anchored seam in the same binary.

## The next trustworthy seam: the hidden export cluster around `MapMgr`

Using the real injected toolbox build's export table, the cluster around `0x1001CC30` is:

- `?CancelEnterChallenge@Map@GW@@YA_NXZ` at `0x1001CC30`
- `?CreateMapContext@Map@GW@@YAPAUMapContext@2@I@Z` at `0x1001CC60`
- `?DestroyMapContext@Map@GW@@YA_NPAUMapContext@2@@Z` at `0x1001CCD0`
- `?EnterChallenge@Map@GW@@YA_NXZ` at `0x1001CCF0`
- `?GetDistrict@Map@GW@@YAHXZ` at `0x1001CD20`

Artifact:

- export scan from the toolbox build, reproduced during this pass

This is a better place to continue because:

- these are real exported entrypoints in the exact loaded binary
- the surrounding hidden code references a consistent runtime-registry cluster
- the same area has several internal helpers that Ghidra partially recognized already

## Hidden helpers immediately before the map exports

Raw disassembly of the same toolbox build shows several small unlabeled helpers right before the exported map cluster.

### Thunk at `0x1001CB90`

```asm
MOV EAX, [0x1008A1A8]
TEST EAX, EAX
JZ  ...
JMP EAX
```

This is a classic nullable indirect-jump wrapper.

### Range-lookup helper at `0x1001CBB0`

This helper walks a table bounded by:

- `DAT_1008A1EC`
- `DAT_1008A1F0`

in `0x0C`-byte steps and compares an input address against `[entry+4, entry+8)`.

If the range matches, it returns `entry[0]`.

That is strong evidence for a small address-range dispatch or metadata table, not a patch-object layout.

### Two-argument callback thunk at `0x1001CBF0`

```asm
MOV EAX, [0x1008A1C4]
...
PUSH ECX
PUSH EDX
CALL EAX
```

Again, this is runtime callback plumbing.

### Thunk at `0x1001CC20`

```asm
MOV EAX, [0x1008A1C0]
TEST EAX, EAX
JZ  ...
JMP EAX
```

So by the time execution reaches the exported map cluster, it is already sitting beside a bundle of callback/dispatch helpers.

## The registry/bootstrap cluster that seeds those globals

A larger hidden initialization cluster lives earlier at `0x1001C780`.

That routine seeds a series of globals:

- `DAT_1008A1C4`
- `DAT_1008A1A8`
- `DAT_1008A1AC`
- `DAT_1008A1B0`
- `DAT_1008A1C0`
- `DAT_1008A1E0`
- `DAT_1008A1E4`
- `DAT_1008A1EC`
- `DAT_1008A1F0`
- `DAT_1008A1F4`

The setup pattern is consistent:

1. build or locate a descriptor with internal helpers
2. turn it into a callable/runtime object
3. store the resulting pointer in one of the globals above

This is much more consistent with:

- runtime registries
- callback vectors
- lookup tables
- helper objects

than with `MemoryPatcher` records.

## Matching cleanup confirms vector/registry semantics

The cleanup routine at [Unwind@1004a1a0](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\full_function_list_latest.log) frees several bounded regions:

- `[DAT_1008A1EC, DAT_1008A1F4)`
- `[DAT_1008A1E0, DAT_1008A1E8)`
- `[DAT_1008A1F8, DAT_1008A200)`

The deallocation pattern is STL-vector-like:

- base pointer
- current/end pointer
- size arithmetic
- free and zero

That fits the range-walk helper at `0x1001CBB0` very well.

## What this means for the earlier patch-owner thread

The safe conclusion is:

- the live client really does have a small `MemoryPatcher` set
- but the absolute-global mapping to `0x10089F18` / `0x10089FA4` / `0x1008A188` is not trustworthy enough in the injected toolbox build
- the hidden registry/dispatch globals around `0x1008A1A8` through `0x1008A1F4` are much better anchored

So the previous patch-owner conclusions should be treated as provisional until they are re-proven against the toolbox binary specifically.

## Strongest result from this pass

The next reliable reverse-engineering seam in the real injected build is now:

- the hidden dispatch/bootstrap cluster around `0x1001C780`
- the unlabeled callback/range helpers at `0x1001CB90`, `0x1001CBB0`, `0x1001CBF0`, and `0x1001CC20`
- the exported `MapMgr` cluster at `0x1001CC30` onward

That is a better continuation path than the fragile patch-owner globals.

## Best next step

The next logical disassembly/decompile pass should be:

1. decompile the exact exported map entrypoints in a fresh project rooted on the toolbox build
2. recover what the seeded globals actually represent:
   - `DAT_1008A1A8`
   - `DAT_1008A1AC`
   - `DAT_1008A1B0`
   - `DAT_1008A1C0`
   - `DAT_1008A1C4`
3. map the range-table records behind `[DAT_1008A1EC, DAT_1008A1F0)` field-by-field
4. then re-check whether any of that machinery touches challenge-entry / map-context behavior directly

At this point, the most valuable progress is not another speculative owner label.
It is build-correct anchoring of the next hidden export seam.

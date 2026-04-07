# GWCA Live Hook Table Addendum

## Scope

This pass uses the live `L I L B I S C U I T` client to dump GWCA's hook table in memory and correlate:

- live hook entries
- live replay-slot addresses
- known detour helper RVAs
- the frame UI hook chain we already reversed statically

The key goal was to stop treating the replay slots as anonymous neighbors and instead map them back to real hook-table entries.

## Tooling update

I extended:

- [live_gwca_runtime_dump.ps1](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\scripts\live_gwca_runtime_dump.ps1)

so it now dumps:

- `DAT_1008b0b4` = hook page-list head
- `DAT_1008b0c0` = hook-table pointer
- `DAT_1008b0c4` = hook-table capacity
- `DAT_1008b0c8` = hook-table count
- each live `0x2c` hook entry with:
  - target
  - detour
  - replay slot
  - flags
  - relocation count
  - saved bytes
  - original/replay offset tables

The raw runtime output is in:

- [live_gwca_runtime_report.txt](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\tests\live_gwca_runtime_report.txt)

## Runtime anchors

From the latest live dump:

- `GwBase = 0x00DE0000`
- `GwcaBase = 0x6ED20000`
- `HookPageList = 0x0A110000`
- `HookTablePtr = 0x1DE00B40`
- `HookTableCap = 0x40`
- `HookTableCount = 0x27`

So this client had:

- one active replay-slot page at `0x0A110000`
- a hook-table heap buffer at `0x1DE00B40`
- `39` active hook entries

That matches the earlier allocator model:

- replay slots come from RWX pages
- hook entries live in a separate heap array

## The frame UI hook is now directly identified in the live table

The critical entry is:

```text
idx=8
target=0x010086D0
detour=0x6ED46860
replay=0x0A110EE0
flags=0x06
reloc=5
```

This maps cleanly to the static work:

- `target 0x010086D0` = live game `SendFrameUIMsg`
- `detour 0x6ED46860` = `gwca + 0x26860` = `FUN_10026860`
- `replay 0x0A110EE0` = the replay slot we already dumped and disassembled

So the frame hook chain is now live-confirmed from the actual table entry, not just from nearby globals:

`hook entry #8 -> target SendFrameUIMsg -> detour FUN_10026860 -> replay slot 0x0A110EE0`

## Neighboring replay slots are now identifiable too

The adjacent replay slots in the same page are no longer anonymous:

### Entry 7

```text
idx=7
target=0x00FED300
detour=0x6ED46470
replay=0x0A110F00
```

Detour RVA:

- `0x6ED46470 - 0x6ED20000 = 0x26470`

This is the already-known `FUN_10026470` detour path.

### Entry 6

```text
idx=6
target=0x00FF02C0
detour=0x6ED468A0
replay=0x0A110F20
```

Detour RVA:

- `0x6ED468A0 - 0x6ED20000 = 0x268A0`

This matches the previously reversed global UI shim:

- `FUN_100268A0`

### Entry 9

```text
idx=9
target=0x012CB250
detour=0x6ED465C0
replay=0x0A110EC0
```

Detour RVA:

- `0x6ED465C0 - 0x6ED20000 = 0x265C0`

This matches the neighboring UI-side detour we had already seen installed from UIModule init:

- `FUN_100265C0`

So the replay page region around the frame hook now has a concrete local map:

- `0x0A110F20` -> entry 6 -> global UI detour `+0x268A0`
- `0x0A110F00` -> entry 7 -> detour `+0x26470`
- `0x0A110EE0` -> entry 8 -> frame UI detour `+0x26860`
- `0x0A110EC0` -> entry 9 -> detour `+0x265C0`

## Other directly identifiable UI-module hooks

The same dump also exposed more of the UIModule-installed hooks we had already seen statically:

### Entry 5

```text
idx=5
target=0x00FAACE0
detour=0x6ED46800
replay=0x0A110F40
```

Detour RVA:

- `0x26800`

This matches:

- `FUN_10026800`

which we had already identified as the Win32 input-activity hook.

### Entry 10

```text
idx=10
target=0x0100ABB0
detour=0x6ED468C0
replay=0x0A110EA0
```

Detour RVA:

- `0x268C0`

This matches:

- `FUN_100268C0`

which was also installed by the UI bootstrap path.

## What the live hook table proves

This runtime dump makes several earlier conclusions much stronger:

1. the replay-slot pool is real and actively used
   - the replay addresses all sit under one live page-list head at `0x0A110000`

2. the `0x2c` hook-entry structure is real and matches the static layout
   - targets, detours, replay slots, flags, and relocation counts all line up

3. the frame/UI detour cluster is not a guess
   - the entries around `0x0A110EE0` are directly tied to the detour RVAs we already reversed

4. the UI bootstrap code at `FUN_10024690` matches live memory
   - its installed detours are visible as real hook entries in the running client

## About the flags and relocation counts

Every hook entry dumped in this run had:

- `flags = 0x06`

which is consistent with the earlier hook-toggle work:

- bit `1` set = enabled
- bit `2` set = desired/latched enabled state
- bit `0` clear = normal 5-byte patch mode, not the special `target-5` / `EB F9` mode

This specific client state therefore did **not** show any active special-mode hook entries.

The frame dispatcher entry also had:

- `reloc = 5`

which matches the earlier replay-stub analysis:

- 5 relocation-table entries were recorded for the frame-dispatch trampoline

## Best current local map of the core UI detour cluster

From this live dump plus earlier static work:

- `+0x26800` -> Win32 input-activity detour
- `+0x26860` -> frame UI dispatcher detour
- `+0x268A0` -> global UI dispatcher detour
- `+0x265C0` -> neighboring UI-side detour installed by UIModule init
- `+0x268C0` -> additional UIModule-installed detour

That is the strongest live-backed view of the UI interception cluster so far.

## Important architectural conclusion

We can now describe the frame hook path at three levels simultaneously:

### Static installer level

- `FUN_10024690` installs the detour
- `FUN_10029730` creates the entry
- `FUN_10029ed0` builds the replay stub
- `FUN_10029470` writes the detour jump

### Table-entry level

- hook entry `#8`
- target `0x010086D0`
- detour `gwca + 0x26860`
- replay `0x0A110EE0`

### Live-byte level

- game entrypoint patched with `jmp` to the GWCA detour
- replay slot begins with displaced original bytes
- replay slot jumps back into the original function after the overwritten bytes

That is about as closed-loop as this reverse path can get without dropping into the game binary itself.

## Best next step

The next logical reverse pass is:

1. identify the remaining unknown detours in the live table by RVA
2. classify the full table by subsystem:
   - UI
   - Render
   - Chat
   - Guild
   - etc.
3. compare the live hook-table population against the MemoryPatcher population so we can finally say which major GWCA features are detour-backed versus byte-patch-backed

At this point, though, the frame/UI replay-slot mapping problem is solved.

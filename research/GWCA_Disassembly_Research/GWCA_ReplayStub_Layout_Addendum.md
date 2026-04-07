# GWCA Replay Stub Layout Addendum

## Scope

This pass continues the GWCA hook-engine reverse work below `FUN_10029730`, with one specific goal:

- explain what the installer-returned replay handle really is
- show where the relocated code lives
- tie the static installer logic to live bytes from an initialized Guild Wars client

This addendum corrects one important over-interpretation from the earlier notes.

## Correction: `FUN_10029ed0` does **not** write into a secondary buffer field

In the earlier read, I described `param_1[2]` inside `FUN_10029ed0` as if it were a field inside the hook record itself.

That is too loose.

The compiled caller at `FUN_10029730` makes the real situation visible:

```cpp
local_2c = param_1;   // target
local_28 = param_2;   // detour
local_24 = piVar3;    // slot returned by FUN_10029d60()
iVar1 = FUN_10029ed0((uint *)&local_2c);
```

So `FUN_10029ed0` receives a temporary 5-dword installer context on the stack:

- `ctx[0]` = target function pointer
- `ctx[1]` = detour function pointer
- `ctx[2]` = replay-slot pointer returned by `FUN_10029d60()`
- `ctx[3]` = patch-mode / special-case flag, filled by `FUN_10029ed0()`
- `ctx[4]` = relocation-entry count, filled by `FUN_10029ed0()`

That means lines like:

```cpp
puVar9 = (undefined1 *)(param_1[2] + uVar4);
```

are writing directly into the executable slot base returned by `FUN_10029d60()`.

The relocated replay code starts at the slot address itself.

## `FUN_10029d60`: the replay handle is a `0x20`-byte executable slot

The allocator is now straightforward to interpret:

- it `VirtualAlloc`s RWX pages of size `0x1000`
- it chains free slots starting at `page + 0x20`
- each slot is `0x20` bytes apart
- it returns one slot address directly

So the returned replay handle is:

- not a heap object
- not a pointer to a separate code blob
- not metadata that then points somewhere else

It is the executable slot address itself.

## `FUN_10029ed0`: what gets written into the slot

The stub writer:

- decodes target instructions with `FUN_1002a1f0(...)`
- rewrites control flow when needed
- copies relocated bytes into `ctx[2] + emitted_offset`
- records mapping entries between original offsets and replay-stub offsets

The key emitted templates are visible in the decompile:

```cpp
local_54 = 0xE8;   // CALL rel32
local_40 = 0xE9;   // JMP rel32
local_4c = 0x800F; // long conditional branch seed
```

So this is a real relocator, not a raw byte copier.

## Compact hook-table entry layout from `FUN_10029730` and `FUN_10029b80`

The installer stores one compact `0x2c`-byte entry per hook in `DAT_1008b0c0`.

The best current layout is:

- `+0x00` target/original function pointer
- `+0x04` detour function pointer
- `+0x08` replay-slot pointer returned by `FUN_10029d60()`
- `+0x0C` saved original bytes or pre-entry bytes, mode-dependent
- `+0x10` additional saved byte/word, mode-dependent
- `+0x14` flag byte
- `+0x18` low nibble = relocation-entry count
- `+0x1C..0x23` original-side instruction offsets
- `+0x24..0x2B` replay-stub-side instruction offsets

What is strongly supported:

- `FUN_10029730` writes the relocation count nibble into `entry+0x18`
- `FUN_10029ed0` fills byte arrays later copied into `entry+0x1C` and `entry+0x24`
- `FUN_10029b80` uses those two byte arrays to translate suspended thread `EIP`s between original code and replay-stub code when hooks are toggled

### Flag-byte interpretation

The flag byte at `entry+0x14` is only partially resolved, but several bits are now meaningful:

- bit `0` stores the `local_20` mode from `FUN_10029ed0`
- bit `1` behaves like current enabled/disabled hook state in `FUN_10029b80`
- bit `2` is consulted as an alternate desired-state source in `FUN_10029b80`, but its exact lifecycle is still not fully pinned down

## Live validation from the `L I L B I S C U I T` client

Using the updated runtime dump script:

- [live_gwca_runtime_dump.ps1](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\scripts\live_gwca_runtime_dump.ps1)

I relaunched the target Guild Wars client, initialized GWCA, and dumped both:

- the patched game `SendFrameUIMsg` entrypoint
- the replay handle stored in `gwca + 0x8A3A0`

The raw output is in:

- [live_gwca_runtime_report.txt](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\tests\live_gwca_runtime_report.txt)

### Runtime anchors

From the latest live dump:

- `GwBase = 0x00DE0000`
- `GwcaBase = 0x6AC70000`
- game `SendFrameUIMsg = 0x010086D0`
- `gwca + 0x8A39C = 0x010086D0`
- `gwca + 0x8A3A0 = 0x0A410EE0`

### Patched game entrypoint

Live bytes at the game entrypoint:

```text
010086D0: E9 8B E1 C8 69 75 08 57 8B F9 ...
```

So the game entrypoint is patched with a `jmp rel32` into GWCA's detour path, exactly as expected from the earlier hook-chain work.

### Replay-stub bytes

The replay handle itself points into a page of `0x20`-byte executable slots:

```text
0A410EE0: 55 8B EC 56 8B 75 08 E9 EB 77 BF F6 00 00 00 00
0A410EF0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0A410F00: 55 8B EC 8B 45 08 E9 FB C3 BD F6 00 00 00 00 00
0A410F10: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0A410F20: 55 8B EC 8B 45 08 E9 9B F3 BD F6 00 00 00 00 00
```

This is a very strong confirmation of the pool model:

- each slot is `0x20` bytes
- the emitted code starts at slot offset `0`
- unused bytes in the slot remain zero
- multiple replay stubs sit back-to-back in the same RWX pool page

## The first replay slot resolves exactly the way the static model predicts

Take the first slot:

```text
0A410EE0: 55 8B EC 56 8B 75 08 E9 EB 77 BF F6
```

Disassembly:

```asm
push ebp
mov  ebp, esp
push esi
mov  esi, [ebp+8]
jmp  0x010086D7
```

The jump destination `0x010086D7` is exactly:

- the live `SendFrameUIMsg` target `0x010086D0`
- plus the `7` original bytes copied into the replay slot

So the replay slot is doing the classic trampoline job:

1. execute the original bytes displaced by the detour
2. jump back into the original function after the overwritten region

That closes the loop between:

- `FUN_10029d60` allocator
- `FUN_10029ed0` relocator
- `FUN_10029730` installer
- `DAT_1008a3a0` replay-handle storage
- `SendFrameUIMessage(...)` calling that handle as if it were a function

## Strongest updated interpretation of `DAT_1008a3a0`

The best current wording is now:

- `DAT_1008a3a0` stores an executable replay-slot address allocated from GWCA's hook-record pool
- the slot begins with relocated original instructions
- it ends with a jump back into the original target after the overwritten bytes

For the frame UI path specifically:

- `DAT_1008a39c` = raw game frame-dispatch target
- `FUN_10026860` = inbound detour
- `DAT_1008a3a0` = callable replay trampoline slot for the original path

## Why `FUN_10029b80` matters here

`FUN_10029b80` gives the relocation tables a concrete purpose.

When toggling hooks, it:

- suspends/inspects thread state
- checks whether `EIP` is inside original or replay-stub code
- uses the byte arrays at `entry+0x1C` and `entry+0x24` to translate `EIP` between those domains

So the relocation metadata is not just build-time bookkeeping.

GWCA uses it at runtime to repair thread instruction pointers safely while changing hook state.

## Best next step

The next logical pass is now narrower and more specific:

1. identify the exact patch-application helper that writes the detour jump into the target
2. resolve the remaining hook-entry flag bits, especially `entry+0x14` bit `2`
3. map the neighboring live replay slots at `0x0A410F00`, `0x0A410F20`, and `0x0A410EC0` back to their owning hooks

At this point, though, the central replay-stub question is no longer speculative.

The live bytes and the static decompilation match.

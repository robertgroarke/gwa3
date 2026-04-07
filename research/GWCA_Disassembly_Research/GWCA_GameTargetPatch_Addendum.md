# GWCA Game-Target Patch Addendum

## Scope

The previous pass identified the three live `MemoryPatcher` objects and recovered their staged bytes.

This pass goes one step deeper:

- load the matching local `Gw.exe`
- convert the live runtime patch targets into static image addresses
- decompile and disassemble the actual game-side code around those targets

That gives us the first direct view of what the patches are bypassing on the Guild Wars side.

## Address normalization

For the live client used in this research:

- runtime `Gw.exe` base = `0x007C0000`
- static PE image base = `0x00400000`

So the three live patch targets map to:

- `0x008AFD06 -> 0x004EFD06`
- `0x009F9362 -> 0x00639362`
- `0x008F898C -> 0x0053898C`

The local image used for the pass was:

- [Gw_livecopy.exe](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\Gw_livecopy.exe)

## New helper scripts

To make the game-image pass practical, I added:

- [DecompileAtList.java](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_scripts\DecompileAtList.java)
- [DisassembleAround.java](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_scripts\DisassembleAround.java)

Those let me decompile multiple target addresses and then inspect the exact instructions at the patch sites.

## Target 0: `0x008AFD06 -> 0x004EFD06`

Static artifacts:

- [Decompile targets log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_gw_patch_targets.log)
- [Disassembly around `0x004EFD06`](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\disasm_004efd06.log)

Live bytes:

```text
patched = EB 0F
original = 8B 45
live = 8B 45
```

Instruction-level context:

```text
004efd06: MOV EAX,[EBP-0x18]
004efd09: MOV ECX,[EBP-0x14]
004efd0c: MOV EDX,[EBP-0x10]
004efd0f: MOV [ESI],EAX
004efd11: MOV [ESI+4],ECX
004efd14: MOV [ESI+8],EDX
```

If the patch is enabled, `EB 0F` at `0x004EFD06` jumps forward to `0x004EFD17`, skipping the final three-float copy-back into `ESI`.

That is a very strong semantic match for a camera/update bypass style patch:

- the function is float-heavy
- it computes interpolated values
- the patch skips writing the computed vector back to the target structure

This is not a perfect source-level proof, but it is the strongest current candidate for the camera-side update patch family.

Confidence:

- game-side behavior: **binary-proven**
- association with `CameraMgr` / unlock-camera semantics: **high-confidence inferred**

## Target 1: `0x009F9362 -> 0x00639362`

Static artifacts:

- [Decompile targets log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_gw_patch_targets.log)
- [Disassembly around `0x00639362`](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\disasm_00639362.log)

Live bytes:

```text
patched = EB
original = 74
live = EB
```

Instruction-level context:

```text
0063935d: FNSTSW AX
0063935f: TEST AH,0x1
00639362: JZ 0x00639381
00639364: PUSH 0x1A9
00639369: MOV EDX,0x00A2319C
0063936e: MOV ECX,0x00A231B8
00639373: CALL 0x00487260
```

Nearby wide string data at `0x00A2319C` resolves to:

```text
" beyond available level data"
```

So this patch forces control flow past an error / validation side path associated with the string:

- `"beyond available level data"`

Semantically:

- original `74` = conditional skip
- patched `EB` = unconditional skip

So the active patch is a hard bypass of a level-data / map-validation branch.

This is the strongest new ownership clue from the game side:

- it does **not** look like chat
- it does **not** look like camera interpolation
- it looks like a world/map/terrain validation bypass

Confidence:

- branch semantics: **binary-proven**
- “level-data / map validation bypass” label: **high-confidence inferred from adjacent string**
- exact owning GWCA module: **still unresolved**

## Target 2: `0x008F898C -> 0x0053898C`

Static artifacts:

- [Decompile targets log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_gw_patch_targets.log)
- [Disassembly around `0x0053898C`](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\disasm_0053898c.log)

Live bytes:

```text
patched = 90 90
original = 75 0C
live = 75 0C
```

Instruction-level context:

```text
00538982: CALL 0x00498E00
00538987: ADD ESP,0x0C
0053898A: TEST EAX,EAX
0053898C: JNZ 0x0053899A
0053898E: PUSH 0x1
00538990: CALL 0x007A1540
00538998: JMP 0x005389F3
```

So when enabled, the `NOP NOP` patch removes the `JNZ` gate entirely and always falls through the failure side.

One especially interesting detail is the nearby callee:

```text
00498E00: ... PUSH 0x10000142 ...
```

And there is nearby string data in the same region including:

- `"GapPorts"`
- `"portList.Count()"`
- `"GmMapInf..."`

That makes this patch look like a map / port / travel-adjacent branch bypass rather than something in chat or camera.

This also weakens the earlier “best candidate is chat” guess from the previous pass.
The compiled chat bootstrap still clearly owns one direct `SetPatch(...)` site, but this particular game-side branch now looks more map/port oriented from the target context.

Confidence:

- branch-removal behavior: **binary-proven**
- “map/port/travel-adjacent path” label: **moderate-confidence inferred**
- exact GWCA owner: **still unresolved**

## What changed from the previous patch-ownership model

The earlier ownership pass said:

- at least one patch was likely chat-owned

After inspecting the real game-side targets, the picture is sharper:

- patch `0x004EFD06` looks strongly camera/update-like
- patch `0x00639362` looks like a level-data / map-validation bypass
- patch `0x0053898C` looks like a map/port/UI-message-adjacent branch bypass

So the memory patcher set now looks less like “mostly chat” and more like:

- one camera-style flow edit
- two world/map-side validation or handling bypasses

That is a more faithful model of the live client state.

## Best next step

The next logical reverse pass is:

1. identify which GWCA bootstrap routine creates the active level-data bypass patch at `0x00639362`
2. identify which bootstrap routine creates the `GapPorts`-adjacent branch patch at `0x0053898C`
3. then update the subsystem matrix with:
   - patch target
   - game-side semantics
   - most likely owning GWCA module
   - confidence

At this point, though, the live patch set is no longer opaque.
We now know what the patched game-side branches actually do.

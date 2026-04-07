# GWCA Patch Object Ownership Addendum

## Scope

The previous pass identified the three live `MemoryPatcher` targets in the game image.

This pass goes back into `gwca.dll` and asks a different question:

- which **global `MemoryPatcher` objects** correspond to those live records
- which compiled GWCA routines reference those globals
- what that lets us say about true patch ownership

This is the first pass that ties live patch **objects** back to specific `gwca.dll` globals instead of only patched game addresses.

## Live object addresses map cleanly into `gwca.dll` globals

From the live client:

- object `0x6EDA9F18`
- object `0x6EDAA188`
- object `0x6EDA9FA4`

and live `gwca.dll` base:

- `0x6ED20000`

the object RVAs are:

- `0x89F18`
- `0x8A188`
- `0x89FA4`

That maps directly to compiled globals:

- `DAT_10089f18`
- `DAT_1008a188`
- `DAT_10089fa4`

This was the key step, because once the live objects are reduced to named data RVAs inside `gwca.dll`, the ownership question becomes a normal cross-reference problem.

## Object `DAT_10089f18` is camera-owned

Static artifacts:

- [refs to `0x10089F18`](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\findrefs_10089f18.log)
- [decompile `FUN_1004a9e0`](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_1004a9e0.log)

The important reference is:

- `UnlockCam @ 0x10003530`

and the matching teardown helper is:

```cpp
if (DAT_10089f28 != '\0') {
    GW::MemoryPatcher::Reset((MemoryPatcher *)&DAT_10089f18);
}
```

That gives a strong binary ownership statement:

- live object `0x6EDA9F18`
- compiled global `DAT_10089f18`
- exported behavior `UnlockCam`

So the live patch object at:

- `target = 0x008AFD06`
- `patched_bytes = EB 0F`

is the camera/unlock-camera patch object.

That upgrades the earlier camera inference from “likely” to effectively **binary-proven at the object-ownership level**.

## Object `DAT_10089fa4` is chat-owned

Static artifacts:

- [refs to `0x10089FA4`](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\findrefs_10089fa4.log)
- [decompile `FUN_10003e30`](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_10003e30.log)
- [decompile `FUN_1004aaa0`](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_1004aaa0.log)

Relevant refs include:

- `FUN_100035d0` which we already mapped to the chat bootstrap cluster
- `FUN_10003e30` which is the matching chat teardown path

The teardown decompile explicitly does:

```cpp
GW::Hook::RemoveHook(... chat hook cluster ...);
GW::MemoryPatcher::Reset((MemoryPatcher *)&DAT_10089fa4);
```

That is the cleanest ownership proof in the whole patch set.

So the live patch object at:

- object `0x6EDA9FA4`
- target `0x008F898C`
- patched bytes `90 90`

is the chat-side patch object.

This is important because the *game-side* target looked map/port-like, which could have been misleading on its own.
The object-level ownership makes the correct conclusion much stronger:

- the patch belongs to the chat subsystem
- even if the guarded game branch lives in a more general UI/map-adjacent code path

## Object `DAT_1008a188` belongs to a third patch-bearing module

Static artifacts:

- [refs to `0x1008A188`](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\findrefs_1008a188.log)
- [decompile `FUN_1004ad50`](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_1004ad50.log)
- [function range around `0x1004A900`](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\listfuncs_1004a900_1004ae50.log)

The matching reset helper is:

```cpp
if (DAT_1008a198 != '\0') {
    GW::MemoryPatcher::Reset((MemoryPatcher *)&DAT_1008a188);
}
```

So this object is definitely a first-class global patch object, not a transient allocation.

What we do **not** yet have is a named exported owner like `UnlockCam` or a clearly named teardown cluster like the chat module.

So the current best statement is:

- live object `0x6EDAA188`
- compiled global `DAT_1008a188`
- owned by a distinct patch-bearing module whose reset helper is `FUN_1004ad50`

At the game-target level, this is also the active patch object:

- target `0x009F9362`
- bytes `74 -> EB`
- adjacent wide string `"beyond available level data"`

So the strongest behavioral inference remains:

- this module owns the active level-data / map-validation bypass patch

but the module name is still unresolved.

## Current ownership matrix

### Patch object `DAT_10089f18`

- live object: `0x6EDA9F18`
- game target: `0x008AFD06`
- live bytes: `8B 45`
- staged patch: `EB 0F`
- owner: camera
- evidence: direct ref from `UnlockCam`
- confidence: **high / binary-proven**

### Patch object `DAT_10089fa4`

- live object: `0x6EDA9FA4`
- game target: `0x008F898C`
- live bytes: `75 0C`
- staged patch: `90 90`
- owner: chat
- evidence: chat bootstrap + chat teardown reset
- confidence: **high / binary-proven**

### Patch object `DAT_1008a188`

- live object: `0x6EDAA188`
- game target: `0x009F9362`
- live bytes: `EB`
- original bytes: `74`
- owner: unnamed third module
- evidence: dedicated reset helper `FUN_1004ad50`
- behavior: active level-data / map-validation bypass
- confidence: **module existence binary-proven, semantic owner still inferred**

## What changed

This pass materially improved the replacement picture:

- one patch object is now definitely camera-owned
- one patch object is now definitely chat-owned
- the last active patch object is confirmed to be separate from those two

That is much better than treating the three live patches as one ambiguous cluster.

## Best next step

The next logical reverse pass is:

1. identify the module table entry or init/exit wrapper that owns `DAT_1008a188`
2. connect that wrapper to the game-side `"beyond available level data"` branch patch
3. then finalize the patch-owner matrix as:
   - object global
   - module
   - game target
   - patch bytes
   - semantic effect
   - confidence

At this point, the patch-object side is mostly solved.

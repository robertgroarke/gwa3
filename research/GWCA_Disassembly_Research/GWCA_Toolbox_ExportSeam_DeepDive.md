# GWCA Toolbox Export Seam Deep Dive

## Scope

This pass continues from the build-correct toolbox seam identified in:

- [GWCA_BuildConsistency_And_ExportSeam_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_BuildConsistency_And_ExportSeam_Addendum.md)

The target binary is the actual injected build:

- [toolbox/GWToolboxpp-master/Dependencies/GWCA/bin/gwca.dll](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\toolbox\GWToolboxpp-master\Dependencies\GWCA\bin\gwca.dll)

The goal of this pass was:

1. anchor the nearby exports in the real toolbox build
2. map which seeded globals back those exports
3. decide whether this region is really ordinary `MapMgr` logic or something lower-level

## The first important result: this seam straddles more than one manager family

The nearby export list is:

- `SetEquipmentVisibility @ 0x1001C830`
- `UseItem @ 0x1001C880`
- `CancelEnterChallenge @ 0x1001CC30`
- `CreateMapContext @ 0x1001CC60`
- `DestroyMapContext @ 0x1001CCD0`
- `EnterChallenge @ 0x1001CCF0`
- `GetDistrict @ 0x1001CD20`

So the hidden bootstrap block around `0x1001C780` is **not** a clean “Map-only” region.
It sits across an `Items` / `Map` export seam.

That matters because it makes the hidden globals around:

- `DAT_1008A1A8`
- `DAT_1008A1AC`
- `DAT_1008A1B0`
- `DAT_1008A1BC`
- `DAT_1008A1C0`
- `DAT_1008A1C4`

more likely to be shared runtime plumbing than plain one-manager statics.

## Exact seeded-global to wrapper mappings

The most stable part of this pass is the exact xref pairing between the hidden bootstrap and the later wrappers.

### `DAT_1008A1AC`

Seeded at:

- `0x1001C7FA`

Used by:

- `0x1001CC30`

Raw wrapper shape:

```asm
MOV EAX, [0x1008A1AC]
TEST EAX, EAX
JNZ  ...
RET
PUSH 0xD6
PUSH 0x10053018
PUSH 0
PUSH [EBP+8]
CALL EAX
RET
```

So whatever semantic name the export has, the compiled body is a descriptor-driven indirect call through `DAT_1008A1AC`.

### `DAT_1008A1B0`

Seeded at:

- `0x1001C82E`

Used by:

- hidden wrapper `0x1001CC90`

Raw wrapper shape:

```asm
MOV EAX, [0x1008A1B0]
TEST EAX, EAX
JNZ  ...
RET
PUSH 0xE0
PUSH 0x10053018
PUSH 0
PUSH [EBP+0xC]
PUSH [EBP+8]
CALL EAX
RET
```

This is the same template family as the previous wrapper, but with a different descriptor/id and two forwarded arguments instead of one.

### `DAT_1008A1BC`

Used by:

- the exported cleanup-heavy routine at `0x1001CCC0`

After iterating callback/listener state, that function does:

```asm
MOV EAX, [0x1008A1BC]
PUSH EDI
CALL EAX
```

So `DAT_1008A1BC` is the final direct implementation target behind that exported seam.

### `DAT_1008A1C0`

Seeded at:

- `0x1001C883`

Used by:

- thunk `0x1001CC20`

Raw body:

```asm
MOV EAX, [0x1008A1C0]
TEST EAX, EAX
JZ ...
JMP EAX
```

This is a pure nullable-jump thunk.

### `DAT_1008A1C4`

Seeded at:

- `0x1001C7AB`

Used by:

- helper `0x1001CBF0`

Raw body:

```asm
MOV EAX, [0x1008A1C4]
...
PUSH [EBP+0xC]
PUSH [EBP+8]
CALL EAX
```

This is a two-argument callback/forwarder thunk.

## The `0x1001CBB0` helper is a range-table lookup

The earlier inference holds up well under closer review.

This helper:

- starts at `0x1001CBB0`
- walks from `DAT_1008A1EC` to `DAT_1008A1F0`
- advances in `0x0C` steps
- compares an input address against:
  - `[entry+4]`
  - `[entry+8]`
- returns `[entry+0]` on match

That makes the record layout effectively:

```text
struct RangeRecord {
    uint32_t value;
    uint32_t begin;
    uint32_t end;
};
```

at least for lookup purposes.

This is the strongest concrete field-level recovery in this pass.

## `0x1001CD20` is the builder for that range table

The other important recovery is that the large routine starting at `0x1001CD20` is not a trivial “getter” body.

It:

1. allocates / prepares temporary workspace
2. iterates a list of pointers gathered into stack scratch space
3. resolves `begin` and `end`-style information for each candidate
4. writes `0x0C`-byte records into the vector bounded by:
   - `DAT_1008A1EC`
   - `DAT_1008A1F0`
   - `DAT_1008A1F4`

The write pattern is explicit:

```asm
MOV [EAX], EDX
MOV [EAX+4], ECX
MOV [EAX+8], EBX
ADD [0x1008A1F0], 0x0C
```

and when the vector is full it calls back into the grow helper at `0x1001C9D0`.

So this seam now has a clean compiled story:

- `0x1001CD20` builds the range records
- `0x1001CBB0` queries them
- `0x1004A1A0` frees them

That is much firmer than just saying “there is probably a registry here.”

## The cleanup side confirms three vector-like regions

The compiled cleanup at [Unwind@1004a1a0](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\full_function_list_latest.log) frees:

- `[DAT_1008A1EC, DAT_1008A1F4)`
- `[DAT_1008A1E0, DAT_1008A1E8)`
- `[DAT_1008A1F8, DAT_1008A200)`

The first of those is now clearly the range-record vector.

The second is the listener/callback object vector used by `0x1001CCC0`.

The third is still unresolved in this pass, but it follows the same allocator/container pattern.

## Most important architectural takeaway

This toolbox seam is **not** behaving like the checked-in source exports.

Instead, it behaves like:

- seeded indirect targets
- small descriptor-driven wrapper stubs
- callback/listener vectors
- range metadata built at runtime

That means the demangled export names in this region are not enough by themselves to tell you what the compiled code is doing.

The strongest safe statement is:

- this is a compiled dispatch/metadata seam that happens to coincide with exported `Items`/`Map` names
- some wrappers likely still serve those public APIs
- but the implementation layer underneath is more generic and runtime-built than the checked-in source suggests

## Source-vs-binary drift note

The checked-in source bodies for:

- [MapMgr.cpp](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\GWCA-master\Source\MapMgr.cpp)
- [ItemMgr.cpp](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWA%20Censured\GWCA-master\Source\ItemMgr.cpp)

do not look like these descriptor wrappers at all.

So for this seam, the compiled toolbox build is now the stronger authority than the local source.

## Best next step

The next logical pass is to keep following the runtime-built metadata, specifically:

1. identify what inputs `0x1001CD20` enumerates before it writes the `RangeRecord` vector
2. resolve the role of the listener vector:
   - `DAT_1008A1E0`
   - `DAT_1008A1E4`
3. determine whether the exported `Map` wrappers are querying this range table for map-context callbacks, challenge-entry handlers, or something more generic

At this point, the seam is no longer “unknown hidden code near MapMgr.”
It is a concrete runtime dispatch layer with:

- indirect targets
- listener vectors
- range records
- explicit grow/free helpers

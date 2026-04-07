# GWCA Toolbox Listener Constructor Addendum

## Scope

This pass continues from:

- [GWCA_Toolbox_RegistryVector_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_Toolbox_RegistryVector_Addendum.md)

The goal was to recover the constructor/inserter side for the listener vector:

- `DAT_1008A1E0 .. DAT_1008A1E4`

This pass succeeds at that structural goal even though some nearby export boundaries remain compiler-noisy in raw disassembly.

## Strongest new result: hidden constructor allocates a fixed `0x30`-byte listener record

The most important recovery comes from raw decode around `0x1001CE50`.

That hidden helper does the following:

1. takes an integer key from the incoming argument
2. calls [GetMissionMapContext](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_1001cf40_recheck.log) to obtain context
3. allocates `0x30` bytes
4. zero-initializes fields through offset `+0x2C`
5. stores the integer key at offset `+0x00`
6. constructs an embedded object at offset `+0x08`
7. appends the resulting heap pointer into the vector at:
   - `DAT_1008A1E0`
   - `DAT_1008A1E4`
   - `DAT_1008A1E8`

The relevant instruction pattern is:

```asm
PUSH 0x30
CALL <allocator>
...
MOV [ESI+0x04], 0
MOV [ESI+0x08], 0
...
MOV [ESI+0x2C], 0
MOV [ESI], EDI
LEA ECX, [ESI+0x08]
CALL 0x100172B0
MOV EAX, [0x1008A1E4]
CMP EAX, [0x1008A1E8]
...
MOV [EAX], ESI
ADD [0x1008A1E4], 0x4
...
MOV ECX, 0x1008A1E0
CALL 0x10003410
```

That is the missing half of the earlier vector story.

## Listener record layout is now materially better defined

Combining this pass with the previous removal/deallocation pass gives:

```text
struct ListenerRecord {
    uint32_t key;          // +0x00
    uint32_t zero_04;      // +0x04, zeroed at creation
    uint8_t  payload[0x24];// +0x08..+0x2B, constructed object area
    void*    callback_ctx; // +0x2C, later consulted and cleared
};
```

This is not yet a full semantic type recovery, but it is enough to say:

- the vector stores pointers to uniform fixed-size records
- the key is first-class and explicit
- `+0x08` is not just padding, it is a constructed subobject
- `+0x2C` is an optional callback/context slot used during teardown/iteration

## The vector itself is a normal growable pointer array

The constructor path confirms the vector triplet:

- `DAT_1008A1E0` = begin
- `DAT_1008A1E4` = current/end
- `DAT_1008A1E8` = capacity end

When there is space:

```asm
MOV [EAX], ESI
ADD [0x1008A1E4], 0x4
```

When there is not:

```asm
PUSH &local_record_ptr
PUSH EAX
MOV ECX, 0x1008A1E0
CALL 0x10003410
```

So the listener side is now cleanly modeled as a vector of `ListenerRecord*`.

## The constructor path is map-context adjacent

One especially useful semantic clue from this pass is that the hidden constructor calls:

- [GetMissionMapContext](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_1001cf40_recheck.log)

which decompiles very simply:

```cpp
Frame* frame = UI::GetFrameByLabel(L"MapWindow");
return UI::GetFrameContext(frame);
```

That does **not** prove the integer key is a map id.
But it does prove this constructor path is operating in a map-window / frame-context neighborhood rather than a generic unrelated heap utility region.

So the best current semantic reading is:

- this listener vector is likely tied to map-window / mission-map style context handling
- not a random global utility registry reused everywhere

## The removal path still matches cleanly

The previous recovery from:

- [FUN_1001db40](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_1001db40_deep.log)

now fits the constructor perfectly:

1. records are matched by `key`
2. callback/context at `+0x2C` is notified if present
3. the heap record is freed
4. the pointer vector is compacted

This means the lifecycle is now closed-loop:

```text
allocate 0x30-byte record
  -> write key
  -> construct subobject at +0x08
  -> append pointer to vector

iterate/remove by key
  -> optional callback on +0x2C
  -> HeapFree(record)
  -> compact vector
```

## Surrounding exports are mixed: not every nearby `Map` export is “weird”

This pass also reinforced an important boundary observation.

At least one nearby exported map helper is completely ordinary:

- [GetMissionMapContext](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_1001cf40_recheck.log)

So the toolbox build is **not** replacing the whole nearby `MapMgr` surface with dynamic registry logic.

Instead, the seam seems to be:

- a localized listener/record system
- sitting beside ordinary map/frame-context wrappers

That is a much more precise model than “the whole region is dynamic dispatch.”

## Best current interpretation

The strongest conservative interpretation after this pass is:

- the listener vector is a map-window-adjacent record registry
- each record is keyed by an integer id
- each record owns a constructed subobject at `+0x08`
- each record may carry an optional callback/context at `+0x2C`
- the exported-heavy seam uses these records before delegating to the final indirect implementation

What remains unresolved is the exact meaning of the integer key and the subobject at `+0x08`.

## Best next step

The next logical pass is to recover the constructor target:

- `0x100172B0`

because that should define the subobject placed at `ListenerRecord + 0x08`.

That is likely the shortest path to the remaining semantics:

1. what the record is really “for”
2. what the integer key identifies
3. what operations the exported-heavy seam is allowing listeners to intercept or mutate

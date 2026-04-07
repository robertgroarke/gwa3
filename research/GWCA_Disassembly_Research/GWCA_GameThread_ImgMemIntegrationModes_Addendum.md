## `Gw.exe` Frame Callback ImgMem Integration Modes Addendum

This pass followed the strongest integration callers of the shared `ImgMem` level-size helper:

- `FUN_0064B2F0`
- `FUN_00690430`
- `FUN_00690620`
- `FUN_00690910`
- `FUN_00690AE0`

The goal was to determine which concrete image-transfer/storage modes actually consume the shared `ImgMem` level-layout logic.

## Source artifacts

These results come from:

- [gw_decomp_imgmem_integration_temp130.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_imgmem_integration_temp130.log)
- [gw_decomp_imgmem_level_temp129.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_imgmem_level_temp129.log)
- [gw_findcallers_0068cc20_temp129.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_0068cc20_temp129.log)

## High-level result

This pass makes the `ImgMem` side look like a small family of related storage/container modes rather than one isolated object.

The best current decomposition is:

- `FUN_00690430(...)`
  - constructor for a six-bank `ImgMem` layout
- `FUN_00690620(...)`
  - grow/rebuild helper for a single-bank level-pointer layout
- `FUN_00690910(...)`
  - validator for a single-bank layout
- `FUN_00690AE0(...)`
  - validator for a six-bank layout
- `FUN_0064B2F0(...)`
  - "add level" integration path on top of one of these layouts

So the shared `FUN_0068CC20(...)` level-size logic is being reused across:

- a single-bank storage object
- a six-bank storage object
- plus live level-add/update validation paths

That is a stronger, more concrete picture than simply "generic image subsystem helper."

## `FUN_00690430(...)`: six-bank constructor

This helper is the most explicit constructor in the group.

### What it does

Its behavior is:

- validate power-of-two dimensions
- clamp requested level count through:
  - `FUN_0068CBB0(...)`
- choose a minimum reserved slot count of:
  - `level_count + 5`
  - but at least `0x0C`
- compute layout with:
  - `FUN_006901E0(..., want_aux = 0, ...)`
- require zero aux-block size from that planner
- allocate an `ImgMem.cpp` object
- then repeat six times:
  - write a bank-local level-pointer table
  - lay out the per-level payload pointers using:
    - `FUN_0068CC20(...)`
- return the constructed object

### Best interpretation

The strongest reading is:

- `FUN_00690430(...)` builds a six-bank multi-level image storage object

That matches the shape of the decompile very strongly:

- one object
- six repeated bank sections
- each bank containing its own level-pointer table plus payload region

## `FUN_00690620(...)`: single-bank grow/rebuild helper

This helper is a good complement to the constructor.

### What it does

Its behavior is:

- validate power-of-two dimensions
- clamp target level count through:
  - `FUN_0068CBB0(...)`
- count current active levels by scanning until a null pointer
- if requested level count differs:
  - if the existing allocation is big enough:
    - allocate a lightweight replacement table
    - repack the level pointers using:
      - `FUN_0068CC20(...)`
  - otherwise:
    - build a fresh object via:
      - `FUN_006902C0(...)`
    - release old per-level slabs as needed
- if level count matches:
  - return existing object unchanged

### Best interpretation

The cleanest reading is:

- `FUN_00690620(...)` is a single-bank resize/rebuild path for an `ImgMem` level-pointer layout

So the single-bank and six-bank forms are not just speculative variants.
They have distinct helper paths.

## `FUN_00690910(...)`: single-bank validator

This helper does not build anything new.
It verifies whether an existing single-bank layout matches the expected format.

### What it does

Its behavior is:

- validate power-of-two dimensions
- require a non-null object
- clamp level count through:
  - `FUN_0068CBB0(...)`
- query total allocated size through:
  - `FUN_0047F050()`
- derive expected pointer-table and payload layout through:
  - `FUN_006901E0(...)`
- verify:
  - each stored pointer offset matches the expected aligned `FUN_0068CC20(...)` walk
  - any remaining reserved slots are null

### Best interpretation

- `FUN_00690910(...)` is the structural validator for the single-bank `ImgMem` layout

That makes it very likely the surrounding cluster uses these validators to recognize or reuse existing image buffers without rebuilding them.

## `FUN_00690AE0(...)`: six-bank validator

This helper is the six-bank analogue of `FUN_00690910(...)`.

### What it does

Its behavior is:

- validate power-of-two dimensions
- clamp target level count through:
  - `FUN_0068CBB0(...)`
- query total object size through:
  - `FUN_0047F050()`
- compute expected single-bank span via:
  - `FUN_006901E0(...)`
- convert that into a six-bank object-size expectation
- then, for each of six banks:
  - verify bank base offset
  - verify each per-level pointer offset against:
    - `FUN_0068CC20(...)`
  - require trailing reserved slots to be null

### Best interpretation

- `FUN_00690AE0(...)` is the structural validator for the six-bank `ImgMem` layout

That pairs very cleanly with `FUN_00690430(...)`.

## `FUN_0064B2F0(...)`: add-level integration path

This helper is the strongest high-level consumer in the set.

### What it does

Its behavior is:

- require an already-initialized object with certain flags set
- require format match
- require the new dimensions to be exactly half the currently stored width/height
- increment an internal level count
- update stored dimensions
- rebuild or extend the backing level layout through:
  - `FUN_00690620(...)`
- compute the new level byte span through:
  - `FUN_0068CC20(...)`
- mark/flush through:
  - `FUN_0046D790(...)`
  - `FUN_0064C2C0()`

### Best interpretation

The strongest reading is:

- `FUN_0064B2F0(...)` is a live "add next downscaled level" path layered over the single-bank `ImgMem` layout

This is a meaningful identity upgrade.
It makes the `ImgMem` family look very much like:

- reusable multi-level image storage
- likely for mip-like or pyramid-like level chains

rather than arbitrary scratch slabs.

## Updated `ImgMem` family model

With this pass included, the current `ImgMem` picture is:

- shared per-level size logic:
  - `FUN_0068CC20(...)`
- shared depth logic:
  - `FUN_0068CBB0(...)`
- shared layout planner:
  - `FUN_006901E0(...)`
- single-bank forms:
  - build/rebuild: `FUN_006902C0(...)`, `FUN_00690620(...)`
  - validate: `FUN_00690910(...)`
- six-bank forms:
  - build: `FUN_00690430(...)`
  - validate: `FUN_00690AE0(...)`
- live integration:
  - add next level: `FUN_0064B2F0(...)`

So the safest current summary is:

- this is a reusable multi-level image-storage subsystem
- with at least one single-bank mode and one six-bank mode
- and live level-add support

## Best next step

The strongest next targets are now:

- `FUN_006A0330`
- `FUN_006A0280`
- `FUN_006A01D0`
- `FUN_006A0630`
- `FUN_0069E870`
- and, if useful, the palette tables:
  - `DAT_00A2C6C8`
  - `DAT_00A2C948`

That should tell us whether the palette-side helpers are using the same single-bank vs multi-bank distinction, and help pin down whether the six-bank mode corresponds to a specific conversion family rather than generic extra channels.*** End Patch

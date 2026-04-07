## `Gw.exe` Frame Callback View Host Region Payload Addendum

This pass continues from the repeated-slot-family note by decompiling the helpers that directly manage the slot `1` host and the slot `2/3` region payloads:

- `FUN_005FD550`
- `FUN_0055DEB0`
- `FUN_005EF650`
- `FUN_0060DAA0`
- `FUN_0060D890`

The goal was to answer two practical questions:

- what kind of geometric/state payload do slots `2` and `3` actually expose?
- what does the slot `1` host lifecycle do when it is recreated or refreshed?

## Source artifacts

These results come from:

- [gw_decomp_view_region_helpers_temp93.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_view_region_helpers_temp93.log)
- [gw_decomp_repeated_slot_family_temp92.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_repeated_slot_family_temp92.log)

## High-level result

This pass confirms the view-host stack more strongly:

- slot `1` is the host/view control
- slots `2` and `3` expose a small geometric/state payload through `0x5A`, `0x5C`, `0x58`, and `0x59`
- host refresh/recreate paths run a deep channel-3 traversal and then a long cleanup/rebuild sequence

So the best current model is now:

- host view control
- paired subordinate region payload controls
- deep owner-local refresh/rebuild path when the host or its nested host anchor changes

## `FUN_005FD550(out, slot_obj)`: `0x5A` geometry/state read

This helper is extremely direct:

```cpp
FUN_00610160(slot_obj, 0x5A, 0, out);
return out;
```

That upgrades the earlier interpretation of `0x5A`.
We already knew `0x5A` was not a plain setter.
Now we can say more concretely:

- it is used as a structured out-read on the slot `2/3` region controls

In the `FUN_005EE080(...)` host callback, `0x5A` is paired with:

- `0x58`
- `0x59`

to return a multi-field region/control state bundle.

So `0x5A` is best described as:

- structured geometry/state query verb

for the subordinate region controls.

## `FUN_0055DEB0(slot_obj, payload)`: `0x5C` geometry/state write

This helper is the setter counterpart:

```cpp
FUN_00610160(slot_obj, 0x5C, payload, 0);
```

In the host callback, `FUN_0055DEB0(...)` is used after converting rectangle edges through:

- `FUN_0046DB80`
- `FUN_0046DBB0`

and pushing the result into slots `2` and `3`.

So `0x5C` is now better described as:

- structured geometry/state write verb

for the subordinate region controls.

This is stronger than the earlier generic “setter” wording.

## What the slot `2/3` payload now looks like

Taken together, the host callback and these helpers now show a consistent protocol on slots `2` and `3`:

- `0x58` => one scalar getter
- `0x59` => another scalar getter
- `0x5A` => structured geometry/state read
- `0x5C` => structured geometry/state write

That is a strong signature for region controls that carry:

- one or more scalar state values
- plus a small structured rectangle/offset payload

So the “paired subordinate region controls” label is holding up well.

## `FUN_005EF650(...)`: host callback bridge for owner-local channels `>= 7`

This helper is tiny but important:

```cpp
if (param_1[1] == 0x31) {
    if (6 < *(uint *)(param_2 + 8)) {
        FUN_006100A0(*param_1, *(uint *)(param_2 + 8), *(undefined4 *)(param_2 + 0xc), param_3);
    }
}
```

So the slot `1` view host uses this callback as a bridge from its incoming event packet into:

- owner-local extended channels `>= 7`

That connects nicely to the earlier extended-channel notes.
The host view is not an isolated widget; it is explicitly wired into the same extended owner-local protocol family we mapped earlier.

## `FUN_0060D890(host_id)`: deep host refresh / rebuild path

This helper is one of the most useful lifecycle bodies in the pass.

At a high level it:

1. validate `host_id`
2. require `FUN_006290B0(host_id) != 0`
3. require current owner flag `4`
4. temporarily clear flag `8` through:
   - `FUN_0062E5D0(8, 0, 0)`
5. if needed, emit owner-local channel:
   - `FUN_00628A10(3, 0, 0)`
6. while channel-3 nodes remain:
   - `FUN_0062D220(3, 0)`
   - `FUN_0060BE80(node)`
7. once exhausted, run a long rebuild/cleanup sequence:
   - `FUN_0062CDD0()`
   - `FUN_006284E0()`
   - `FUN_0061F680()`
   - `FUN_0061B210()`
   - `FUN_006243E0()`
   - `FUN_0062F6F0()`
   - `FUN_00613C00()`
   - `FUN_0062F630()`
   - `FUN_0062EFF0()`
   - `FUN_0062E680()`
   - `guard_check_icall()`
   - `FUN_0062CB00()`
   - `FUN_00629280()`
   - `FUN_00626420()`
   - `FUN_00627890()`
   - `FUN_00624180()`
   - `FUN_006210F0()`
   - `FUN_0061F200()`
   - `FUN_0061A560()`
   - `FUN_00618160()`
   - `FUN_006162C0()`
   - `FUN_00614CD0()`
   - `FUN_005A6CFB(host_obj, 0x1C8)`

That is much stronger than the earlier generic “refresh” wording.
This is a full deep rebuild / teardown-refresh sequence rooted in the host object.

## `FUN_0060DAA0(host_id)`: broader host-anchor refresh path

This helper is closely related to `FUN_0060D890(...)`, but its scope is a little broader.

The body is almost the same, except:

- it first walks:
  - `FUN_0062D220(3, 0)`
  - over a wider channel-3 family
- and then applies the same deep rebuild sequence to each matching region/host node it encounters

So the safest distinction is:

- `FUN_0060D890(host_id)` = refresh one concrete host object
- `FUN_0060DAA0(host_id)` = refresh host-related channel-3 dependents anchored under a broader owner/root

That matches how the earlier host callback used `FUN_0060DAA0(slot0)` when rebuilding the nested child under slot `0`.

## What this says about slot `0`

The last note already suggested slot `0` was an auxiliary or nested host anchor.

This pass strengthens that:

- `FUN_0060DAA0(slot0)` runs a broader host refresh path
- then a new child is created under slot `0` through `FUN_0060D300(...)`

So slot `0` now looks even more like:

- auxiliary nested host/anchor control

rather than a plain optional decorative child.

## Updated host/region model

After this pass, the cleanest working model is:

- slot `1`
  - main host/view control
  - bridges owner-local channels `>= 7`
- slot `0`
  - auxiliary nested host/anchor control
  - participates in broader host refresh/recreate flow
- slot `2`
  - subordinate region control with:
    - `0x58`
    - `0x59`
    - `0x5A`
    - `0x5C`
- slot `3`
  - second subordinate region control with the same general protocol shape

This is materially stronger than the earlier “host + regions” wording because the protocol surface is now explicit.

## Strongest current interpretation of the family

The best current label for this specialized family is:

- view host with paired interactive region payload controls and nested host anchor support

That is verbose, but it is a much closer description of the recovered behavior than:

- list
- tabs
- generic entries

## Best next step

The highest-yield next pass is to keep following the host-specific rebuild chain and the region payload interpreters:

- `FUN_0060BE80`
- `FUN_0059A110`
- `FUN_005EB2A0`
- `FUN_005EF470`
- `FUN_0060E4C0`

Those should let us say:

- what the channel-3 rebuild walker is actually doing to host/region nodes
- and whether the slot `2/3` payloads represent rectangles, scroll ranges, viewport offsets, or another more specific geometry/state type  

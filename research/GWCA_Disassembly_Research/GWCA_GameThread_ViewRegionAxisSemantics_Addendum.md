## `Gw.exe` Frame Callback View Region Axis Semantics Addendum

This pass continues from the view-host region-payload note by decompiling the helpers most directly tied to the slot `2/3` update path in `FUN_005EF470(...)`:

- `FUN_0060BE80`
- `FUN_0059A110`
- `FUN_005EB2A0`
- `FUN_005EF470`
- `FUN_0060E4C0`
- `FUN_005EDBD0`
- `FUN_0062F6A0`
- `FUN_0046DB60`

The goal was to answer a narrower question than before:

- are slots `2` and `3` just generic subordinate payload controls?
- or are they behaving like axis-bearing region controls that accept directional deltas?

## Source artifacts

These results come from:

- [gw_decomp_region_semantics_temp94.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_region_semantics_temp94.log)
- [gw_decomp_region_followups_temp95.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_region_followups_temp95.log)
- [gw_findcallers_005edbd0_temp95.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_005edbd0_temp95.log)
- [gw_findcallers_0062f6a0_temp95.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_0062f6a0_temp95.log)

## High-level result

This pass makes the slot `2/3` interpretation more specific.

The strongest current model is now:

- slot `1` is still the main view host
- slots `2` and `3` still look like paired subordinate region controls
- but those two region controls now look axis-like, because the host drag/update path computes normalized directional motion and pushes signed scalar deltas into slot `2` and slot `3` separately

So the safest upgrade is:

- not just "paired region payload controls"
- but "paired axis-bearing region controls" or "paired directional region controls"

I still would not over-name them as scrollbars or camera axes yet, but they are clearly more structured than generic child payload slots.

## `FUN_0060BE80(owner)`: recursive channel-3 subtree rebuild

This helper is very close to `FUN_0060D890(...)`, but it is recursive over the channel-3 subtree.

At a high level it:

1. requires owner flag `4`
2. clears flag `8` through:
   - `FUN_0062E5D0(8, 0, 0)`
3. if flag `8` was not previously set, emits:
   - `FUN_00628A10(3, 0, 0)`
4. repeatedly walks:
   - `FUN_0062D220(3, 0)`
   - and recursively applies `FUN_0060BE80(...)`
5. once the subtree is exhausted, runs the same long rebuild sequence seen in the host-refresh path

So this is best described as:

- recursive channel-3 subtree rebuild walker

That matters because it strengthens the earlier reading of channel `3` as a real owner-local refresh/rebind family, not just a loose event id.

## `FUN_0059A110(slot_obj, value)`: `0x5B` scalar setter

This helper is direct:

```cpp
FUN_00610160(slot_obj, 0x5B, value, 0);
```

This adds a new region-control verb:

- `0x5B` => scalar setter

Because `FUN_005EF470(...)` uses `0x5B` on slots `2` and `3` after directional-motion processing, `0x5B` looks like an axis/value update verb rather than a general-purpose metadata setter.

## `FUN_005EB2A0(slot_obj, value)`: `0x5E` scalar setter

This helper is equally direct:

```cpp
FUN_00610160(slot_obj, 0x5E, value, 0);
```

So the slot-family protocol now includes another scalar setter:

- `0x5E` => scalar setter with a distinct role from `0x5B`

I would still avoid assigning exact semantics to `0x5E` from this pass alone, but it belongs in the same scalar-control band as `0x57`, `0x5B`, and `0x5C`.

## `FUN_005EDBD0(state, profile, pos)`: normalized motion classifier

This helper is the real semantic upgrade of the pass.

It:

- updates current and previous positions
- stores raw deltas in `state[5]` and `state[6]`
- in mode `1`, computes adjusted local motion relative to the stored origin
- when `profile[5] != 0.0`, derives normalized directional components from helper outputs and clamps/interpolates through the `profile` range
- writes scaled directional magnitudes into `state[7]` and `state[8]`
- promotes `*state` from `1` to `2` when the motion magnitude exceeds the configured threshold `profile[0]`

That is much stronger than a plain "did the mouse move?" helper.

This looks like:

- gesture or directional-motion classifier
- with thresholded promotion from idle/armed state to active drag state
- and with two directional output components

So the slots `2/3` path in `FUN_005EF470(...)` is now much easier to read as:

- classify motion
- choose active directional target
- convert directional delta
- write axis-specific updates into slot `2` and/or slot `3`

## `FUN_005EF470(...)`: slot `2/3` directional update path

This helper now reads more cleanly.

Important behaviors:

- it only runs while `*(this + 0x40) != -1`
- it uses `FUN_005EDBD0(...)` to update directional state
- it computes a motion vector from floats at:
  - `this + 0x30`
  - `this + 0x34`
- it chooses slot `3` first when one normalized component passes threshold `0.7`
- otherwise it falls back to slot `2` if the other component passes threshold `0.7`
- if neither axis qualifies, it resets the active target and clears the state

Then, once `*(this + 0x1C) == 2`, it performs concrete writes:

- slot `2` gets `FUN_0059A110(slot2, -FUN_0046DB60(this->x_delta))`
- slot `3` gets `FUN_0059A110(slot3,  FUN_0046DB60(this->y_delta))`

That is the clearest slot `2/3` evidence so far.

The safest interpretation is:

- slot `2` and slot `3` are orthogonal directional targets
- `0x5B` applies signed scalar deltas to those targets
- the host is routing drag-like directional motion into those two region controls separately

I still do not want to over-commit to exact names like:

- horizontal scrollbar / vertical scrollbar
- pan-x / pan-y
- left-edge / top-edge

But the binary now supports "paired orthogonal directional controls" much more strongly than the older generic region label.

## `FUN_0062F6A0(id)`: active-target clear helper

This helper is tiny but useful:

```cpp
if (DAT_00bb5658 == id) {
    DAT_00bb5658 = -1;
    DAT_00bd0dd0 = 0;
    FUN_00625cb0();
}
```

So `FUN_005EF470(...)` is not just choosing arbitrary child ids.
It is interacting with a shared active-target slot:

- `DAT_00bb5658`

and clearing that state when the selected directional target is released or invalidated.

That strengthens the reading of the whole path as:

- target acquisition
- directional drag/update
- target release

rather than a passive property poller.

## `FUN_0046DB60(value)`: conversion wrapper

This helper decompiles only as:

```cpp
FUN_005aec3b(value);
thunk_FUN_005a7290();
```

So this pass does not yet resolve the exact numeric conversion, but its usage in `FUN_005EF470(...)` is still informative:

- it sits directly between float directional delta and `0x5B` scalar write
- its output is signed and passed as the slot update magnitude

That makes it very likely this helper is doing one of:

- float-to-int rounding
- scaled fixed-point conversion
- unit normalization for the target control family

## `FUN_0060E4C0(out, obj_id, bounds)`: measurement wrapper

This helper is just:

```cpp
FUN_00628800(obj_id);
FUN_00629E80(out, bounds);
return out;
```

So it confirms one small but useful point:

- the host/region family is still using the same general measurement pipeline we mapped earlier

This helper does not add new slot semantics by itself, but it keeps the slot family tied to the shared relation/layout engine rather than a side subsystem.

## Updated region-control protocol

After this pass, the region-control surface looks like:

- `0x58` => scalar getter
- `0x59` => scalar getter
- `0x5A` => structured geometry/state read
- `0x5B` => signed scalar delta/value setter
- `0x5C` => structured geometry/state write
- `0x5E` => another scalar setter

That is a much more credible control protocol for:

- paired orthogonal region axes
- or paired directional subcontrols under the view host

than for generic anonymous child payload nodes.

## Updated working model

The cleanest current working model is:

- slot `1`
  - main view host
  - owns the motion/gesture path
  - bridges extended owner-local channels
- slot `2`
  - first directional region control
  - accepts signed scalar updates via `0x5B`
- slot `3`
  - second directional region control
  - accepts signed scalar updates via `0x5B`
- slot `0`
  - auxiliary nested host/anchor control
  - still participates in broader rebuild flow

So the stack now looks less like:

- host + generic region payload children

and more like:

- host + paired directional region controls

## Best next step

The next best step is to chase the remaining numeric and caller context around this path:

- `FUN_005AEC3B`
- `FUN_005A7290`
- `FUN_0052CC30` as the other caller of `FUN_005EDBD0(...)`

That should tell us whether the slot `2/3` updates are best named as:

- scroll/pan axes
- drag-handle offsets
- or a more general normalized directional-control protocol


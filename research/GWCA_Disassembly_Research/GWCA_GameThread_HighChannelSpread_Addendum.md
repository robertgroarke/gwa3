# GWCA GameThread High Channel Spread Addendum

This pass continues from the sharp-seam pivot note by answering the next obvious structural question:

- are owner-local high typed channels mostly a single control-family protocol?
- or does the same `FUN_006100A0(...)` band show up across clearly different systems?

Fresh targets for this pass were a deliberately spread caller sample:

- `FUN_004D06D0`
- `FUN_004FDB40`
- `FUN_00514140`
- `FUN_00535770`
- `FUN_00557A40`

## Source Artifacts

These results come from:

- [gw_findcallers_006100a0_temp185.log](c:\Users\Robert\Documents\GWA Censured X BotsHub\tools\ghidra_projects\gw_findcallers_006100a0_temp185.log)
- [gw_decomp_highband_spread_temp187.log](c:\Users\Robert\Documents\GWA Censured X BotsHub\tools\ghidra_projects\gw_decomp_highband_spread_temp187.log)

## Main Result

This pass answers the breadth question cleanly.

The owner-local high typed-channel band is not confined to one directional-control pocket. The same channel family now appears in at least four distinct-looking control domains:

- character-creation item/color control
- generic `UiCtlInstance`-style control instances
- bounded numeric/value controls
- relation-driven stateful controls with slot refresh/requery behavior

So the strongest current model is:

- `FUN_006100A0(...)` is a broad owner-local control/content protocol surface
- individual channel ids still keep family-local semantics
- but the band itself is not narrow or one-off

That is stronger than the earlier phrasing:

- "high channels are live"

because we can now say:

- high channels are reused across multiple distant subsystems

## Caller Spread

### `FUN_004D06D0(...)`: character-creation item/color control uses channels `7` and `8`

This caller names:

- `P:\\Code\\Gw\\Ui\\Game\\CharCreate\\CrItemColor.cpp`

and uses the high band in a very concrete small protocol.

On incoming `0x31` packets:

- when `param_2[4] == 0` and `param_2[8] == 8`
  - call:
    - `FUN_006100A0(owner, 7, 1, 0)`
- when `param_2[4] == 0` and `param_2[8] == 9`
  - call:
    - `FUN_006100A0(owner, 7, 0, 0)`
- when `param_2[4] == 2` and `param_2[8] == 7`
  - assemble a small local payload
  - call:
    - `FUN_006100A0(owner, 8, &local, 0)`
  - then potentially re-arm through:
    - `FUN_006100A0(owner, 7, 1, 0)`

So this is not the earlier directional motion family repeating itself.

It is a different UI domain reusing the same high-band pair as:

- state-toggle / arm-like channel `7`
- payload-bearing commit / finish-like channel `8`

### `FUN_00514140(...)`: `UiCtlInstance`-style control uses `8`, `10`, and `0x0B`

This caller names:

- `P:\\Code\\Gw\\Ui\\Controls\\UiCtlInstance.h`

and shows another distinct high-band sequence.

For one high message path:

- call:
  - `FUN_006100A0(owner, 0x0B, 0, &out_ptr)`
- if the out-pointer remains null:
  - mark a local state bit
  - call:
    - `FUN_006100A0(owner, 10, 0, 0)`

For a sibling path:

- reset a pair of slots / scales
- then call:
  - `FUN_006100A0(owner, 8, 0, 0)`

So here the band is clearly participating in:

- mode-end or release check through `0x0B`
- directional/update continuation through `10`
- and a reset/end-like signal through `8`

That aligns with the earlier interaction-lifecycle notes, but now from a different control shell.

### `FUN_00535770()`: bounded numeric control uses `7 / 8 / 9`

This caller does not look like the drag/motion family at all.

It:

- reads current owner flags through `FUN_00610EA0(...)`
- derives a bounded value from engine globals
- clamps that value against local min/max state
- stores the new current and effective values

Then, if the value changed:

- if the effective cap changed:
  - `FUN_006100A0(owner, 7, new_cap, 0)`
- always:
  - `FUN_006100A0(owner, 8, new_value, 0)`
- if the value remains below the cap:
  - `FUN_006100A0(owner, 9, 0, 0)`

This is especially useful because it proves the high band also serves scalar/value controls, not just gesture or interaction hosts.

The cleanest reading here is:

- `7` = changed limit / bound / envelope
- `8` = changed current committed value
- `9` = value-below-limit follow-up / secondary state signal

Those exact names remain provisional, but the structure is clearly not a drag-only protocol.

### `FUN_00557A40(...)`: relation-driven stateful control uses `9 / 10 / 0x0B / 0x0C`

This caller is another strong spread point because it mixes owner-local high channels with relation lookups and slot operations.

For one branch:

- on incoming relation event `7`
  - update two owner-relative slot controls
  - trigger:
    - `FUN_00610540(owner)`
    - `FUN_00610330(owner)`
  - then call:
    - `FUN_006100A0(owner, 10, payload, 0)`

For another branch:

- on relation event `8` with zero payload
  - call:
    - `FUN_006100A0(owner, 0x0B, 0, 0)`

For another:

- on relation event `7` whose nested payload reports `9`
  - call:
    - `FUN_006100A0(owner, 9, 0, 0)`

And on a final branch:

- on relation event `9`
  - call:
    - `FUN_006100A0(owner, 8, 0, 0)`
  - if `FUN_0060F610(owner) != 0`
    - call:
      - `FUN_006100A0(owner, 0x0C, payload, 0)`

So this control is using:

- `9`
- `10`
- `0x0B`
- `0x0C`

inside a relation-driven update path rather than a plain direct interaction path.

That is strong evidence that the high band is a real shared owner-local protocol surface, with family-specific meanings layered on top.

### `FUN_004FDB40(...)`: control-instance seam still anchors the old interpretation

This decompile mostly reconfirms the earlier `CtlInstance` interaction-family work rather than opening a new branch.

It still sits over the same instance-backed control path and remains useful mainly as a consistency check:

- the old interaction-lifecycle interpretation still holds
- the newer caller samples above are genuinely additive rather than contradictory

## What This Changes

Before this pass, the best safe reading was:

- channels `7+` are a live owner-local high band
- and at least one directional interaction family uses:
  - `7`
  - `8`
  - `9`
  - `10`
  - `0x0B`

After this pass, the stronger reading is:

- the high band is broad
- the same small id range is reused across multiple control families
- and the semantics of a given id are partly shared and partly family-local

So the best current layered model is:

- low typed channels
  - traversal / lifecycle / relation plumbing
- high typed channels
  - shared owner-local control/content protocol band
- concrete id meaning
  - refined by the specific control family using it

That is a much safer abstraction than trying to force one global name onto:

- `7`
- `8`
- `9`
- `10`
- `0x0B`
- `0x0C`

across every caller.

## Updated Interpretation

The strongest current interpretation is now:

- `FUN_006100A0(...)` provides a broad owner-local notification plane for higher-level controls
- some ids probably carry shared abstract roles:
  - begin / arm
  - commit / current value
  - follow-up / content update
  - directional update
  - end / release
- but each control family specializes those roles to its own local state machine

So the engine is not exposing:

- one global "extended interaction protocol"

It is exposing something more flexible:

- one broad owner-local high-channel notification plane
- reused by many higher-level control families

## Best Next Step

The next best reverse step is to tighten one level down from this spread result:

1. follow one or two more callers that use:
   - `0x0C`
   - or `0x0B`
   - to see whether those ids are confined to relation-driven control families
2. pair this with:
   - `FUN_0060F610(...)`
   - because `FUN_00557A40(...)` uses it as the gate before channel `0x0C`
3. only after that, decide whether the high-band needs:
   - a family-by-family matrix appendix
   - or just one smaller note for the `0x0B / 0x0C` tail

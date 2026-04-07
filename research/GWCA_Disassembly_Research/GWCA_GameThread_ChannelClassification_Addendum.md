## `Gw.exe` Frame Callback Channel Classification Addendum

This pass continues from the typed-phase lookup note by decompiling the highest-yield unresolved callers of `FUN_00628A10(...)`:

- `FUN_0060D300`
- `FUN_0060BE80`
- `FUN_0060BCB0`
- `FUN_0060BD30`

The goal here was to turn the current channel family from:

- “typed relation channels exist”

into:

- “these specific typed channels correspond to these concrete owner behaviors”

## Source artifacts

These results come from:

- [gw_decomp_phase_callers_temp72.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_phase_callers_temp72.log)

## High-level result

This pass fills in the channel family much more concretely.

The strongest new points are:

- `type_id 2` is now clearly tied to an owner/frame setup/initialization path
- `type_id 1` is now clearly a veto-gated transition/preflight path
- `type_id 3` is reinforced as the activation/rebind channel
- and the global message ids `7` and `10` show up as companion notifications around those owner-local channels

So the typed channel family is no longer just:

- `3 = rebind`
- `4 = reevaluation`

It now looks more like:

- `1 = transition preflight/veto`
- `2 = setup/enter/initialization`
- `3 = activation/rebind/drain`
- `4 = relation/layout reevaluation`

That is a much stronger taxonomy.

## `FUN_0060D300(...)` and `FUN_0060BD30(...)`: owner/frame setup channel

These two functions are effectively sibling setup paths.

They both:

- reference:
  - `P:\\Code\\Engine\\Frame\\FrApi.cpp`
  - line `0x10F`
- run a long initialization/reset sequence
- clear several fields in the newly prepared object:
  - `+0x84`
  - `+0x88`
  - `+0x8C`
  - `+0x90 = 0x40`
- call:
  - `FUN_006240D0()`
  - `FUN_00627740(param_3)`
  - `FUN_00629190()`
  - `FUN_0062C650(...)`
  - `FUN_0062E520()`
  - `FUN_00613400(...)`
  - `FUN_0060BBD0()`
  - `FUN_0062EFE0()`
  - `FUN_0062F580()`
  - `FUN_00627D70(param_4, param_5)`

and then, crucially:

- `FUN_0062E5D0(4, 0, 0)`
- `FUN_00628A10(2, 0, 0)`
- `FUN_006286D0(10, 0, 0)`

That combination is the important part.

### What this says about `type_id 2`

`type_id 2` now looks very strongly like:

- an **owner/frame setup-enter channel**

Why:

- it happens at the end of a broad initialization path
- it follows explicit object setup/reset work
- it is paired with global message `10`

So the best current interpretation is:

- `type_id 2` = owner-local setup/enter notification channel

and:

- global message `10` = setup/enter side broadcast in the same family

The exact user-visible name is still open, but “setup/enter” is now a much stronger label than a generic “phase 2”.

## `FUN_0060BCB0(owner)`: veto-gated transition path

This is the clearest new classification win in the whole pass.

Its behavior is:

1. capture `owner + 0xBC`
2. initialize a local veto/status flag to `0`
3. send:
   - `FUN_006286D0(7, 0, &status)`
4. if the owner is still valid:
   - if `status != 0`, abort and return `0`
   - else call:
     - `FUN_00628A10(1, 0, &status)`
5. if the owner is still valid:
   - if `status != 0`, abort and return `0`
   - else continue into:
     - `FUN_0060BE80(owner)`

That gives `type_id 1` a much stronger identity:

- it is a **blockable owner-local preflight/transition channel**

And it also sharpens message `7`:

- `0x07` is a **global preflight/veto broadcast**

The two planes line up neatly here:

- first global preflight (`7`)
- then owner-local preflight (`type_id 1`)
- then, if not vetoed, proceed into the activation/rebind path

That is an unusually clean structure.

## `FUN_0060BE80(owner)`: activation/rebind channel confirmed

This function was already partially known through the earlier `FUN_0060D890(...)` path, but here it stands alone and confirms the reading.

Its behavior is:

- require owner
- validate owner flags/categories with `FUN_0062E640(4)` and `FUN_0062E640(8)`
- raise owner flag `8` through:
  - `FUN_0062E5D0(8, 0, 0)`
- if category `8` was not already active:
  - `FUN_00628A10(3, 0, 0)`
- while:
  - `FUN_0062D220(3, 0)` returns another owner
  - recurse/iterate through `FUN_0060BE80(...)`
- then run the large refresh cascade

That makes `type_id 3` even stronger:

- **owner-local activation/rebind/drain channel**

It is not merely “some rebind-ish phase.” It is the channel used to activate state, walk all participating owners in that channel, and then refresh dependent systems.

## Channel family so far

With this pass added, the best current classification is:

### `type_id 1`

- owner-local preflight / transition veto channel
- paired with global message `7`
- used by `FUN_0060BCB0(...)`

### `type_id 2`

- owner-local setup / enter / initialization channel
- paired with global message `10`
- used by `FUN_0060D300(...)` and `FUN_0060BD30(...)`

### `type_id 3`

- owner-local activation / rebind / drain channel
- used by `FUN_0060BE80(...)` and indirectly by `FUN_0060D890(...)`

### `type_id 4`

- owner-local relation/layout reevaluation channel
- used by `FUN_0062A980(...)`

That is now a very usable phase/channel map.

## What this does to the layered model

Before this pass, the phase taxonomy had a good structural split but still lacked concrete names for several channels.

After this pass, the layered model is much stronger:

### Global message plane

- `0x07` = global preflight/veto broadcast
- `0x10` = global setup/enter broadcast
- `0x25` = active-owner changed
- `0x26` = global relation cleanup/reset broadcast
- `0x29` = global relation deactivation/clear broadcast

### Owner-local typed channel plane

- `1` = preflight/veto
- `2` = setup/enter
- `3` = activation/rebind/drain
- `4` = relation/layout reevaluation

### Gated wrapper plane

- `FUN_006287D0(...)`
- `FUN_00628570(...)`
- `FUN_00628740(...)`

That is now coherent enough to reason about behavior, not just storage.

## Best current interpretation

The strongest safe reading after this pass is:

- the frame/relation seam that GWCA hooks includes a full owner lifecycle:
  - preflight
  - setup/enter
  - activation/rebind
  - layout/relation reevaluation
  - cleanup/deactivation

And that lifecycle is split cleanly across:

- global broadcasts
- owner-local typed channels
- lower callback/handler dispatchers

That is a much more sophisticated model than a generic “UI update callback.”

## Best next step

The next best reverse step is to keep filling in the remaining channel family and verify whether any more channel ids are in active use.

The highest-yield targets now are:

- `FUN_0062F7E0`
- `FUN_0060DAA0`
- `FUN_006100A0`
- `FUN_0060C3A3`

Those all call `FUN_00628A10(...)` and should tell us:

- whether `type_id 0` or `type_id 5` are real live channels
- whether there are explicit “exit”, “hover”, “drag”, or “commit” channels
- and how much of the frame/relation lifecycle we have already covered

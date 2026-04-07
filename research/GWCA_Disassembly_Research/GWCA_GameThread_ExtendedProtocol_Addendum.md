## `Gw.exe` Frame Callback Extended Protocol Addendum

This pass continues from the extended-channel usage note by decompiling the helper cluster immediately around the live extended ids `7`, `8`, `9`, and `10`:

- `FUN_00610160`
- `FUN_00610370`
- `FUN_0060F610`
- `FUN_00570F40`
- `FUN_00570EC0`
- `FUN_00572120`

The goal was to move the extended cluster from:

- “these ids are live”

to:

- “this cluster behaves like a structured protocol around secondary frame objects and high-id message dispatch”

## Source artifacts

These results come from:

- [gw_decomp_extended_cluster_temp77.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_extended_cluster_temp77.log)

## High-level result

This pass sharpens the extended cluster in three useful ways.

First:

- `FUN_00610160(...)` is not another owner-local typed-channel helper
- it is a **generic high-id message forwarder** into `FUN_006286D0(...)` for ids `>= 0x56`

Second:

- `FUN_00610370(...)` allocates/initializes a secondary object via:
  - `FUN_006289F0()`
  - `FUN_00628E00(...)`

Third:

- the sampled extended callers use ids `7/8/9/10` as a real multi-step control protocol around that secondary object and its message stream

So the strongest current model is:

- the extended owner-local ids `7..10` sit alongside a high-id global message subprotocol (`0x56`, `0x57`, `0x58`, `0x59`, `0x5D`)
- and together they look much more like a structured control/configuration layer than a generic callback range

## `FUN_00610160(owner_id, msg_id, arg3, arg4)`: generic high-id global message forwarder

This helper is extremely revealing.

Its behavior is:

- require `owner_id != 0`
- require `msg_id >= 0x56`
- validate the owner
- call:
  - `FUN_006286D0(msg_id, arg3, arg4)`

This is important because it clearly separates two different kinds of ids:

- owner-local typed channels:
  - `FUN_00628A10(type_id, ...)`
- high-id global message plane:
  - `FUN_00610160(owner_id, msg_id >= 0x56, ...)`

That means the extended cluster is not just “typed channels 7..10.” It is mixing:

- owner-local extended ids `7..10`
- with a companion high-id global message family `>= 0x56`

This is a key architecture point.

## `FUN_00610370(owner_id, cb, arg)`: secondary object allocation/init

This helper:

- validates the owner
- gets a new object from:
  - `FUN_006289F0()`
- initializes it with:
  - `FUN_00628E00(new_obj, cb, arg)`
- and returns the new object handle

That makes the setup callers from the previous pass easier to interpret:

- they are not only creating owners
- they are also creating/configuring a secondary associated object and then talking to it through the high-id message plane

So this is one of the structural anchors for the extended cluster.

## `FUN_0060F610()`: minimal validity probe

This helper is tiny:

- just `FUN_006290B0()`

By itself it does not add much meaning, but in context it confirms the sampled extended callers were using it as:

- a quick “is the owner/object still valid?” gate

before moving into the stronger rebind path.

## `FUN_00570EC0(id)`: mode/category resolver

This helper resolves a numeric category/mode for a given id.

Its behavior is:

- if the id matches one special current object from `FUN_007E4B30()`
  - resolve through `FUN_007E4770(id)` and return `+0x14`
- otherwise iterate another table through:
  - `FUN_007E5A30(...)`
  - `FUN_007E5AB0(...)`
- and return `+0x0C`
- if nothing matches, return `0x0B`

So the extended protocol callers are using a real mode/category classification, not only raw ids.

That matters because several of the control flows branch on:

- `mode == 0x0B`

which now reads like a meaningful category sentinel rather than a magic number with no context.

## `FUN_00570F40()`: extended cluster controller / mapper

This is the richest function in the batch and probably the most important one for naming the extended cluster.

Its high-level shape is:

1. resolve a category via:
   - `FUN_00570EC0(current_id)`
2. map one input subtype into another compact code:
   - `0 -> 0`
   - `1 -> 0`
   - `2 -> 1`
   - `3 -> 4`
   - `4 -> 2`
   - `5 -> 6`
   - `6 -> 3`
   - `7 -> 5`
3. allocate/resolve a secondary object via:
   - `FUN_0060E2B0(owner, 4)`
4. send a high-id message:
   - `FUN_00610160(obj, 0x59, &local_30, 0)`
5. enumerate candidate records via:
   - `FUN_0081F110(...)`
6. match/choose a record based on:
   - the mapped subtype
   - mode/category
   - optional extra object info
7. possibly call:
   - `FUN_00572120(...)`
8. finish with:
   - `FUN_00610160(obj, 0x58, chosen_value, 0)`

This is a much stronger signal than the earlier “extended ids are paired.”

This helper is acting like:

- a controller that maps one local category to another
- configures a secondary object
- chooses a matching candidate/configuration
- and commits that choice through high-id messages `0x59` and `0x58`

So the extended cluster now looks much more like:

- **selection/configuration of a secondary frame/control object**

than like arbitrary extended event ids.

## `FUN_00572120(ctx, array, count)`: multi-target high-id configuration sequence

This helper strengthens that reading.

Its behavior is:

- iterate three target groups via a table at `DAT_009235EC`
- for each group:
  - resolve a target object through `FUN_0060E2B0(...)`
  - enable it with `FUN_00610D30(obj, 1)`
  - send:
    - `FUN_00610160(obj, 0x59, ctx, 0)`
- then, depending on the provided array and compatibility checks:
  - `FUN_00610160(obj, 0x58, chosen, 0)`
  - `FUN_00610160(obj, 0x56, 0, &local_8)`
  - maybe `FUN_00610160(obj, 0x57, 0, &local_14)`
- finally disable/cleanup with:
  - `FUN_00610D30(obj, 0)`
  - and eventually `FUN_00610540(owner)`

That is not generic event handling. It is a structured per-object configuration protocol.

The high-id messages now have a stronger behavioral feel:

- `0x59` = push context / setup packet
- `0x58` = choose/set one candidate/value
- `0x56` / `0x57` = query/commit/status-style follow-up steps

The exact names are still inferential, but the protocol shape is very strong.

## What this does to the extended ids `7..10`

The sampled callers from the previous pass used:

- `7`
- `8`
- `9`
- `10`

as a sequence around this helper family.

With the new helper bodies in hand, the strongest safe interpretation is:

- those extended owner-local ids are not the whole story by themselves
- they are coordinating a **secondary object configuration/control protocol**
- whose actual work then flows through high-id global messages like:
  - `0x56`
  - `0x57`
  - `0x58`
  - `0x59`
  - `0x5D`

So the extended cluster is better described as:

- **owner-local control-phase ids feeding a high-id secondary-object message protocol**

That is much more precise than just calling them “extended interaction channels.”

## Updated protocol model

The strongest current layered picture is now:

### Low-end owner traversal

- `0`, `1`, `2`, `5`

### Mid-range owner lifecycle

- `2`, `3`, `4`, `6`

### Extended owner-local control ids

- `7`, `8`, `9`, `10`

### High-id secondary-object message protocol

- `0x56`
- `0x57`
- `0x58`
- `0x59`
- `0x5D`

This suggests the system is not just owner selection and relation maintenance. It also includes a layered configuration protocol for associated frame/control objects.

## Best current interpretation

The strongest safe reading after this pass is:

- the GWCA-hooked frame/relation seam sits above a multi-layer UI/control framework
- owner-local channels choose lifecycle and control phases
- then high-id messages configure or query secondary objects tied to that owner

So the extended cluster is now better understood as:

- **a control/configuration protocol layered on top of the relation-owner framework**

not just “more event ids.”

## Best next step

The next best reverse step is to identify what the secondary objects from:

- `FUN_006289F0()`
- `FUN_0060E2B0(owner, slot)`

actually are.

The highest-yield targets now are:

- `FUN_006289F0`
- `FUN_00628E00`
- `FUN_00610D30`
- `FUN_00610540`
- and one or two callers of `FUN_0060E2B0(...)`

That should let us answer whether this high-id protocol is configuring:

- a specific frame class
- a widget/control family
- a modal panel
- or another layer of owner-attached UI objects

## `Gw.exe` Frame Callback Generation Front Door Addendum

This pass followed the next layer under the `CtlTextMl` worker family:

- `FUN_00610F60`
- `FUN_0060CE70`
- `FUN_005F8810`
- `FUN_005EB280`
- `FUN_005F9400`

The goal was to determine whether `FUN_00610F60(...)` is:

- the real row/record producer
- or just the shared front door into an even deeper generation engine

## Source artifacts

These results come from:

- [gw_decomp_generation_layer_temp110.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_decomp_generation_layer_temp110.log)

## High-level result

This pass produced a useful boundary shift.

`FUN_00610F60(...)` is not the final producer logic.
It is the shared owner-local generation front door.

Its role is:

- validate/setup owner and input state
- prepare a result buffer descriptor
- enter owner-local generation context through `FUN_00628800(owner)`
- call the deeper worker:
  - `FUN_00614220(...)`
- interpret the returned rect/result buffer
- reduce that result to width/height for the caller

So the real deeper generation seam is now:

- `FUN_00614220(...)`

and `FUN_0060CE70(...)` is the sibling path that:

- calls the same deep worker
- then immediately consumes the newly generated records through `FUN_006163E0(...)`

That is a cleaner and more concrete pivot than we had before.

## `FUN_00610F60(...)`: shared generation entrypoint

The function body is small but informative.

Its flow is:

1. validate owner/context inputs
2. copy or seed a local result descriptor
3. ensure descriptor storage is usable
4. call:
   - `FUN_00628800(param_2)`
   - `FUN_00614220(param_3, param_4, param_5, param_6, param_7, param_8, 0, &local_30, 0)`
5. interpret the returned rectangle in `local_2c`
6. return only:
   - width
   - height

That means `FUN_00610F60(...)` is best understood as:

- "run owner-local generation and return resulting extent"

not:

- "construct the row/record set itself"

The critical detail is the argument handoff:

- all of the content/selector/style inputs are forwarded to `FUN_00614220(...)`
- the locally managed structure at `&local_30` is the output/control descriptor

So the production layer clearly continues below this function.

## `FUN_0060CE70(...)`: sibling generate-and-consume path

This function confirms the shared-backend model.

Its flow is:

1. validate owner/context inputs
2. call `FUN_00628800(owner)`
3. prepare a local descriptor block if none was supplied
4. remember the prior generated-count value
5. call:
   - `FUN_00614220(...)`
6. if new generated entries were added, call:
   - `FUN_006163E0(delta_count, base + old_count * 4, param_11, owns_local_descriptor)`

So unlike `FUN_00610F60(...)`, which reduces the result to extents, `FUN_0060CE70(...)`:

- runs the same deeper generation engine
- then processes the newly generated output records immediately

That makes the relationship much clearer:

- `FUN_00610F60(...)` = generate and measure
- `FUN_0060CE70(...)` = generate and consume produced entries

## `FUN_005F8810(...)`: concrete repeated-child / text-button family callback

This function turned out to be a real control implementation, not a low-level generator.

The strongest constructor evidence is:

- case `9` allocates from:
  - `P:\\Code\\Engine\\Controls\\CtlTextBtn.cpp`

So this is a concrete `CtlTextBtn`-style control family.

### Important cases in `FUN_005F8810(...)`

Case `8`:

- reads state from the control body at `param_1[2]`
- calls:
  - `FUN_0060CE70(...)`
- optionally cleans up with:
  - `FUN_005F8780(...)`

This is the strongest confirmation that `FUN_0060CE70(...)` is the backend generation/consumption path used by this concrete control.

Case `0x38`:

- calls:
  - `FUN_00610F60(...)`
- writes the resulting width/height into the caller-provided out block

This is the matching "measure only" path.

So within one concrete control we now have both roles visible:

- `0x38` -> `FUN_00610F60(...)` for size query
- case `8` -> `FUN_0060CE70(...)` for generate/consume/update

That is excellent structural evidence that both wrappers sit above the same deep engine.

### Other control-surface details from `FUN_005F8810(...)`

This control also exposes a useful message surface:

- `0x57` -> get field at `+0x18`
- `0x58` -> get field at `+0x1C`
- `0x59` -> fetch current UTF-16 content at active index
- `0x5A` -> remaining-count style query
- `0x5B` -> set field `+0x18` and dirty owner
- `0x5C` -> set field `+0x20`, dirty owner, notify owner
- `0x5D` -> set field `+0x1C` and dirty owner
- `0x5E` -> propagate a value at `+0x34` across attached child objects
- `0x5F` -> replace backing text buffer from full source text
- `0x60` -> replace/truncate backing text buffer from bounded source text

And it participates directly in the owner-local interaction protocol through messages like:

- `0x20`, `0x22`, `0x24`, `0x25`, `0x2C`, `0x2E`, `0x39`, `0x4C`, `0x56`

So again, this is clearly a concrete control family that consumes the generation backend, not the hidden producer itself.

## `FUN_005EB280(...)` and `FUN_005F9400(...)`: thin verb forwarders

These two helpers are simple and useful for naming:

- `FUN_005EB280(control, value)`
  - `FUN_00610160(control, 0x5D, value, 0)`
- `FUN_005F9400(control, int value, extra)`
  - packages `{value, extra}`
  - `FUN_00610160(control, 0x60, &local_c, 0)`

That means the repeated-child materializer from the last pass was configuring concrete `CtlTextBtn` children through ordinary control verbs, not by manually writing their internals.

## What this pass changes

Before this pass, `FUN_00610F60(...)` was the strongest candidate for the deeper record builder.

After this pass, the cleaner picture is:

- `FUN_00610F60(...)` is a shared front door into generation
- `FUN_0060CE70(...)` is the sibling front door that additionally consumes generated deltas
- `FUN_00614220(...)` is now the strongest deeper target
- `FUN_006163E0(...)` is the strongest immediate post-generation consumer target

So the real producer boundary moved one layer down again, but this time in a much more precise way.

## Updated working model

The cleanest current stack is now:

- concrete controls like `CtlTextMl` / `CtlTextBtn`
- control-local helpers like `FUN_005ECEB0(...)` and `FUN_005F8810(...)`
- shared generation entrypoints:
  - `FUN_00610F60(...)`
  - `FUN_0060CE70(...)`
- deeper generation engine:
  - `FUN_00614220(...)`
- post-generation consumer:
  - `FUN_006163E0(...)`

That is a much stronger decomposition than we had before.

## Best next step

The next best step is to decompile:

- `FUN_00614220`
- `FUN_006163E0`
- `FUN_00613D10`
- `FUN_005F8780`

Why these are now the right targets:

- `FUN_00614220(...)` is the likeliest true row/record producer
- `FUN_006163E0(...)` consumes the newly generated entries right after production
- `FUN_00613D10(...)` looks like the text-buffer reload/update helper used by `CtlTextBtn`
- `FUN_005F8780(...)` is the state cleanup path paired with the generate/consume branch

So the shortest route to the actual production format now runs through `FUN_00614220(...)`, not through the concrete text control families anymore.

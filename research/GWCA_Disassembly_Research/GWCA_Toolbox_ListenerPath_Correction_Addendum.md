# GWCA Toolbox Listener Path Correction Addendum

## Scope

This pass corrects the earlier toolbox-seam thread from:

- [GWCA_Toolbox_ListenerConstructor_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_Toolbox_ListenerConstructor_Addendum.md)
- [GWCA_Toolbox_Subobject_Correction_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_Toolbox_Subobject_Correction_Addendum.md)
- [GWCA_Toolbox_VTable_Boundary_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_Toolbox_VTable_Boundary_Addendum.md)

The main result is that the earlier `0x1001CE50 -> 0x100172B0` seam was wrong for the toolbox build.

The real listener path is:

```text
FUN_10024690 (UI bootstrap)
  -> FUN_1001da50 (register listener record)
      -> FUN_1001db40 (remove prior record by key)
      -> operator_new(0x30)
      -> FUN_10017eb0 (assign embedded callable-holder from stack seed object)
      -> append into DAT_1008A1E0..DAT_1008A1E8

FUN_1001d8c0 (intercepting wrapper)
  -> iterate DAT_1008A1E0..DAT_1008A1E4
  -> call listener callback at record[+0x2C] if present
  -> call DAT_1008A1BC underlying target
```

## Correction 1: `0x1001CE50` is not the listener constructor

Raw decode and the function list agree that:

- `0x1001CE50` is `GetIsMapUnlocked`
- `0x100172B0` is a small frame-message getter wrapper in this toolbox build

That means the earlier claim that `0x1001CE50` allocated a `0x30`-byte listener record and called `0x100172B0` as an embedded subobject constructor is superseded.

Supporting artifacts:

- [full_function_list_latest.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\full_function_list_latest.log)
- [findrefs_10080184_temp.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\findrefs_10080184_temp.log)
- [disasm_10080184_temp.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\disasm_10080184_temp.log)

## Correction 2: the real constructor is `FUN_1001da50`

Fresh decompilation from a clean project gives the actual constructor:

- [decomp_1001da50_temp.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_1001da50_temp.log)

Recovered behavior:

1. call `FUN_1001db40(key)` to remove any existing record for the same key
2. allocate `0x30` bytes with `operator_new(0x30)`
3. zero fields `+0x04 .. +0x2C`
4. write the key at `+0x00`
5. call `FUN_10017eb0(record + 0x08, &stack_arg_object)`
6. append the record pointer into:
   - `DAT_1008A1E0`
   - `DAT_1008A1E4`
   - `DAT_1008A1E8`
7. destroy the temporary stack object with `FUN_10001c00`

That closes the constructor side with much stronger evidence than the earlier raw-byte inference.

## The record layout is now stable again

The corrected record layout is:

```text
struct ListenerRecord {
    uint32_t key;           // +0x00
    uint32_t zero_04;       // +0x04
    uint8_t  holder[0x24];  // +0x08 .. +0x2B
    void*    callback_ctx;  // +0x2C
};
```

This still matches the earlier removal/iteration findings at a field level.

What changed is the identity of the constructor and the callable-holder helper.

## Correction 3: the embedded assignment helper is `FUN_10017eb0`, not `0x100172B0`

The real callable-holder copy/ownership helper is:

- [decomp_10017eb0_temp.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_10017eb0_temp.log)

This function does exactly what the earlier misattributed analysis was trying to describe:

- clone/copy from `param_1 + 0x24`
- use local inline buffers
- swap ownership with `this + 0x24`
- invoke virtual slots `+0x00`, `+0x04`, and `+0x10`
- clean up old state correctly

So the earlier **semantic** conclusion survives:

- the subobject at `ListenerRecord + 0x08` is function-object / erased-callback style storage

But the earlier **address attribution** does not:

- it is `FUN_10017eb0`
- not `0x100172B0`

## `FUN_1001db40` really is the removal path

The earlier removal analysis still holds and now fits the corrected constructor perfectly.

Caller confirmation:

- [findcallers_1001db40_temp.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\findcallers_1001db40_temp.log)

So the lifecycle is now:

```text
register(key, seed)
  -> remove existing key
  -> allocate 0x30-byte record
  -> assign embedded callable holder
  -> append to listener vector

remove(key)
  -> find record by key
  -> callback through +0x2C if present
  -> free record
  -> compact vector
```

## `FUN_1001d8c0` is the live interceptor wrapper

Fresh decompilation:

- [decomp_1001d8c0_temp.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_1001d8c0_temp.log)

Recovered behavior:

1. `GW::Hook::EnterHook()`
2. iterate `DAT_1008A1E0 .. DAT_1008A1E4`
3. for each record:
   - read `record[+0x2C]`
   - if non-null, call virtual slot `+0x08` with `&param_1`
4. call underlying target in `DAT_1008A1BC`
5. call `FUN_1001d920()`
6. `GW::Hook::LeaveHook()`

This makes the listener vector’s role explicit:

- it is on the live intercepted call path
- not just a side registry

## Bootstrap ownership: only `FUN_10024690` seeds this listener path

The cleanest ownership result in this pass comes from:

- [findcallers_1001da50.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\findcallers_1001da50.log)
- [decomp_10024690_temp.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_10024690_temp.log)

`FUN_1001da50` has one recovered caller:

- `FUN_10024690`

And `FUN_10024690` ends with:

1. scanning/hooking the UI subsystem
2. storing `_clock()`
3. calling `FUN_10030303(...)`
4. calling `FUN_1001da50((uint *)&DAT_1008a351)`

That means this listener path is bootstrap-owned by the same UI setup function that installs the frame/UI detour cluster.

## The seed object is stack-built at the call site

Raw decode of the tail of `FUN_10024690` shows that before calling `FUN_1001da50`, the bootstrap builds a temporary object directly on the stack:

- vtable-like pointer at `+0x00` = `0x10053D20`
- callable target at `+0x04` = `0x100265E0`
- self/holder pointer at `+0x24` = object base
- separate pushed key/token = `0x1008A351`

Artifacts:

- [decomp_10024690_temp.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\decomp_10024690_temp.log)
- [findcallers_100265e0_temp.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\findcallers_100265e0_temp.log)

So the most likely corrected calling convention is conceptually:

```text
FUN_1001da50(key_token, stack_seed_object)
```

even though Ghidra flattens the signature awkwardly.

## Best current interpretation

The strongest conservative reading after this correction pass is:

- the toolbox listener seam is real
- but the earlier addresses were wrong
- `FUN_1001da50` registers one bootstrap-seeded listener record
- `FUN_10017eb0` manages the embedded callable-holder at `record + 0x08`
- `FUN_1001d8c0` is the live wrapper that dispatches through the listener vector before calling the underlying target

So the semantic model is now stronger and the address-level model is cleaner.

## Best next step

The next logical reverse step is to resolve the seeded callable body at:

- `0x100265E0`

That should answer the last important question in this seam:

- what the bootstrap-installed listener actually does to the intercepted argument before `DAT_1008A1BC` is invoked

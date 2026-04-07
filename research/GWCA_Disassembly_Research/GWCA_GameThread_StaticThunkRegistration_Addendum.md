# GWCA GameThread Static Thunk Registration Addendum

This pass finally converts the “static installer blob” idea into concrete installed records.

The key result is that the unlabeled regions are not passive constructor tables. They are real code thunks: tiny one-purpose registration functions that push key values, call a registry-record constructor, stamp a post-construction table pointer into the target object, and then call a common notifier/helper.

That gives us the first installed callback records directly, not just their lookup and constructor logic.

Supporting logs:

- [gw_disasm_0045d6e0_temp151.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_disasm_0045d6e0_temp151.log)
- [gw_disasm_0045d6e0_wide_temp152.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_disasm_0045d6e0_wide_temp152.log)
- [gw_findcallers_0068f540_temp149.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findcallers_0068f540_temp149.log)
- [gw_findrefs_ptra2595c_temp148.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\gw_findrefs_ptra2595c_temp148.log)

## 1. The `0x0045D6EB..0x0045DB3B` region is code, not data

The earlier caller map to `FUN_0068F540(...)` looked strange because Ghidra had no function boundaries there.

The fresh disassembly shows what is really happening:

```asm
0045d6e0  PUSH 0x1
0045d6e2  PUSH 0x0
0045d6e4  PUSH 0x8
0045d6e6  MOV  ECX,0x00bdb458
0045d6eb  CALL 0x0068f540
0045d6f0  PUSH 0x902920
0045d6f5  MOV  dword ptr [0x00bdb458],0x00a276f8
0045d6ff  CALL 0x005a7266
0045d704  POP  ECX
0045d705  RET
```

So the region is a run of tiny thunks:

- load keys
- choose destination object storage
- call the constructor/inserter
- overwrite the object’s first field with a final table pointer
- notify/finalize through `0x005A7266`
- return

That is much stronger than “maybe a static table.”

## 2. Concrete installed `ImgFlip` callback record

The thunk above gives us the first fully concrete installed record for the `FUN_0068F540(...)` registry:

- destination object:
  - `0x00BDB458`
- constructor:
  - `FUN_0068F540(...)`
- constructor key triple:
  - `(8, 0, 1)`
- post-construction table pointer:
  - `0x00A276F8`

That means at least one installed `ImgFlip` callback record is explicitly:

- key0 = `8`
- key1 = `0`
- key2 = `1`

with a final dispatch table rooted at `0x00A276F8`.

That is the first real installed record instance we have recovered for this family.

## 3. The pattern strongly suggests many more `FUN_0068F540(...)` thunks nearby

The disassembly at the start of the region already shows the next thunk beginning immediately after:

```asm
0045d710  PUSH 0x1
0045d712  PUSH 0x0
0045d714  PUSH 0x10
...
```

Even without the full next body decoded here, the pattern is already obvious:

- each thunk differs mainly in the pushed key triple and destination object
- each one creates one installed callback record instance

That matches the earlier evenly spaced ref pattern almost perfectly.

So this region is best understood as:

- a static registration thunk run for `FUN_0068F540(...)` records

## 4. No defined functions in `0x0045D600..0x0045DC50`

The range scan found:

- `TOTAL_LISTED = 0`

for that whole region.

That matters because it explains why the earlier xrefs looked like `<no function>`.

So the model is now:

- this is genuine executable code
- but currently undecorated / unlifted by Ghidra as function objects

That is exactly the kind of thing you often see in startup registration thunks.

## 5. The broad transfer registry has the same static-thunk pattern

The wider disassembly also exposed the sibling pattern for `FUN_00688720(...)`.

Examples:

```asm
0045c5a0  PUSH 0x1
0045c5a2  PUSH 0x11
0045c5a4  PUSH 0x11
0045c5a6  MOV  ECX,0x00bda8c4
0045c5ab  CALL 0x00688720
...
```

```asm
0045c6f0  PUSH 0x1
0045c6f2  PUSH 0x14
0045c6f4  PUSH 0x14
0045c6f6  MOV  ECX,0x00bda9c0
0045c6fb  CALL 0x00688720
...
```

```asm
0045c750  PUSH 0x1
0045c752  PUSH 0x0
0045c754  PUSH 0x1
0045c756  MOV  ECX,0x00bdaa08
0045c75b  CALL 0x00688720
...
```

So the broad transfer-manager registry is installed by the same overall mechanism:

- tiny static thunks
- explicit pushed keys
- target object storage in globals
- post-construction vtable/table patch

That is a strong symmetry result with the `ImgFlip` registry.

## 6. Post-construction table pointer patching is real

This is an especially important detail.

The thunks do not stop after calling the constructor.

They also do:

```asm
MOV dword ptr [object], final_table_ptr
```

For the `FUN_0068F540(...)` thunk we saw:

- constructor’s base table:
  - `PTR_DAT_00A2595C`
- final installed table:
  - `0x00A276F8`

So the constructor is very likely creating a shared base-node shape, and the thunk then patches in the specific concrete dispatch table for that installed record.

That is a major upgrade in our model.

It means:

- `PTR_DAT_00A2595C` is best read as a shared base table / base-node vtable
- the final per-record dispatch table can differ by installed instance

So the concrete family behavior probably lives in those patched final tables like:

- `0x00A276F8`
- and the neighboring `0x00A276??` values used by sibling thunks

## 7. Why this matters for the callback-family question

This is the strongest progress yet toward binding registry records to concrete family callbacks.

Before this pass, we had:

- constructor logic
- lookup logic
- family callback bodies

But no installed record instances.

Now we have at least one explicit installed record:

- created by `FUN_0068F540(...)`
- key triple `(8, 0, 1)`
- final table pointer `0x00A276F8`

And we know the neighboring thunks are doing the same thing with different key triples.

So the next step is no longer vague. It is to mine this thunk run for:

- all key triples
- all destination objects
- all final table pointers

Then compare those final table pointers or their neighboring method slots against the concrete family callbacks already recovered.

## 8. Best current model

The callback-installation architecture now looks like:

1. static startup thunk
   - pushes keys
   - chooses target global
2. base constructor/inserter
   - `FUN_00688720(...)` or `FUN_0068F540(...)`
3. shared base-node vtable
   - `PTR_DAT_00A2595C`
4. post-construction patch
   - overwrite object `+0x00` with final dispatch table
5. common post-registration helper
   - `0x005A7266`

That is a much better model than “some constructors are probably called at startup.”

## 9. Strongest next step

The strongest next reverse step is now very specific:

- enumerate the static thunks in the `FUN_0068F540(...)` run
- extract their pushed key triples and final table pointers
- then inspect those final tables around `0x00A276F8` and its neighbors

That should let us connect:

- installed registry entries
- to actual final dispatch tables
- and then to the concrete `DXT1`, `DXT2/3`, `DXT4/5/DXTL`, and `DXTA` family methods

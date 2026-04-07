# GWCA GameThread Static Table Slice Addendum

This pass pushes one step past the static installer-thunk discovery and resolves what those patched table pointers actually are.

## Main result

The static thunks are not patching records to isolated per-record vtables. They are patching them into contiguous function-pointer banks, often by advancing 4 bytes at a time through a shared method array.

That means the compiled pattern is:

1. call a generic registry-record constructor/inserter like `FUN_0068F540(...)` or `FUN_00688720(...)`
2. overwrite `record + 0x00` with a final dispatch-table pointer
3. use different offsets into one shared function-pointer bank to specialize behavior

So `PTR_DAT_00A2595C` looks even more like a temporary/base node table now, while the real runtime behavior comes from the patched final table pointer.

## ImgFlip registry: shared function bank

The `ImgFlip` installer run around `0x0045D6E0` makes the slice pattern explicit.

Observed thunks:

- `0x0045D6E0`: `FUN_0068F540(0x8, 0x0, 0x1)` into `0x00BDB458`, then patch `0x00BDB458 = 0x00A276F8`
- `0x0045D710`: `FUN_0068F540(0x10, 0x0, 0x1)` into `0x00BDB47C`, then patch `0x00BDB47C = 0x00A276FC`
- `0x0045D740`: `FUN_0068F540(0x18, 0x0, 0x1)` into `0x00BDB4A0`, then patch `0x00BDB4A0 = 0x00A27700`

Dumping pointers at `0x00A276F8` shows the shared bank directly:

- `0x00A276F8 -> FUN_0068F130`
- `0x00A276FC -> FUN_0068F1B0`
- `0x00A27700 -> FUN_0068F230`
- `0x00A27704 -> FUN_0068F2B0`
- `0x00A27708 -> FUN_0068F330`
- `0x00A2770C -> FUN_0068F3A0`
- `0x00A27710 -> FUN_0068F420`
- `0x00A27714 -> FUN_0068F4D0`

Immediately after that bank is the source-path string data:

- `0x00A27718`: `"P:\\Code\\Engine\\Gr\\Img\\ImgFlip.cpp"` (ASCII bytes visible in the dump)

That is a very strong anchor. The installed `ImgFlip` callback records are pointing into a shared `ImgFlip.cpp` method array, and the installer selects a starting slot by patching progressively later addresses in that bank.

## Broad transfer registry: same specialization pattern

The broader transfer registry shows the same compiled style.

Dumping pointers at `0x00A26C14` yields a contiguous function-pointer bank:

- `0x00A26C14 -> FUN_006743A0`
- `0x00A26C18 -> FUN_00674B20`
- `0x00A26C1C -> FUN_00675260`
- `0x00A26C20 -> FUN_00675E80`
- `0x00A26C24 -> FUN_00676A40`
- `0x00A26C28 -> FUN_006775A0`
- `0x00A26C2C -> FUN_006780B0`
- `0x00A26C30 -> FUN_00678A70`
- `0x00A26C34 -> FUN_00678AE0`
- `0x00A26C38 -> FUN_00678B50`
- `0x00A26C3C -> FUN_00678C70`
- `0x00A26C40 -> FUN_00678E40`
- `0x00A26C44 -> FUN_00679000`
- `0x00A26C48 -> FUN_00679150`
- `0x00A26C4C -> FUN_006792F0`
- `0x00A26C50 -> FUN_00679490`
- `0x00A26C54 -> FUN_00679610`
- `0x00A26C58 -> FUN_006797C0`
- `0x00A26C5C -> FUN_00679930`
- `0x00A26C60 -> FUN_00679AA0`
- `0x00A26C64 -> FUN_00679BE0`
- `0x00A26C68 -> FUN_00679D70`
- `0x00A26C6C -> FUN_0067A210`
- `0x00A26C70 -> FUN_0067A6B0`

Earlier static thunks already showed entries like:

- `(0x11, 0x11, 0x1) -> 0x00A26C14`
- `(0x11, 0x11, 0x3) -> 0x00A26C18`
- `(0x12, 0x12, 0x1) -> 0x00A26C1C`
- `(0x12, 0x12, 0x3) -> 0x00A26C20`
- `(0x13, 0x13, 0x1) -> 0x00A26C24`
- `(0x13, 0x13, 0x3) -> 0x00A26C28`
- `(0x13, 0x13, 0x5) -> 0x00A26C2C`

So this registry is also specializing records by offsetting into a shared callback-method bank instead of allocating unique per-entry tables.

## Interpretation

The registry picture is sharper now:

- generic constructor/inserter:
  - writes key fields
  - installs base node table `PTR_DAT_00A2595C`
  - inserts into a hash/list registry
- static installer thunk:
  - rewrites `record + 0x00` to a final dispatch table pointer
- final dispatch pointer:
  - often points into a shared contiguous method bank
  - different records may point at different offsets inside that same bank

That means the compiled registry system is even more table-driven than it first looked. The static registration thunks are effectively selecting family-specific method slices out of prebuilt callback banks.

## Best next step

The next highest-value move is to inspect one of these patched banks as a real method family rather than just a pointer array:

- either keep going on the `ImgFlip` bank and map what each slot means structurally
- or do the same for the broader transfer bank starting at `0x00A26C14`

At this point, though, the construction/install story is no longer ambiguous: the static installer region is wiring registry records onto shared callback-table slices.

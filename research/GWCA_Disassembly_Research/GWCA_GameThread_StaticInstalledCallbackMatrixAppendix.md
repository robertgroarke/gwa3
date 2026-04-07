# GWCA GameThread Static Installed Callback Matrix Appendix

This appendix turns the static registration region into a concrete installed-callback model.

The earlier callback-installation notes already established:

- shared registry-node base records
- constructor / lookup pairs for two callback registries
- and strong evidence for static installer thunks

The remaining question was:

- what do those static thunks actually install?

The answer is now much stronger than before:

- the installer thunks create real registry records with explicit key triples
- patch those records onto contiguous callback-method banks
- and, on the broad transfer side, those bank slices are already concrete compressed-format worker kernels

This is a synthesis note rather than a fresh decompilation pass.

## Source Notes

This appendix consolidates:

- [GWCA_GameThread_CallbackRegistryInstallationAppendix.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_CallbackRegistryInstallationAppendix.md)
- [GWCA_GameThread_StaticThunkRegistration_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_StaticThunkRegistration_Addendum.md)
- [GWCA_GameThread_StaticTableSlice_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_StaticTableSlice_Addendum.md)
- [GWCA_GameThread_TransferBankKernelSlices_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_TransferBankKernelSlices_Addendum.md)
- [GWCA_GameThread_ImgFlipCallbackFamilies_Addendum.md](c:\Users\Robert\Documents\GWA Censured X BotsHub\research\GWCA_Disassembly_Research\GWCA_GameThread_ImgFlipCallbackFamilies_Addendum.md)

## Core Installation Pattern

The static thunks now read as a real compiled installation scheme:

1. push key values
2. choose a destination global object
3. call a generic registry-record constructor/inserter
   - `FUN_0068F540(...)`
   - or `FUN_00688720(...)`
4. overwrite `record + 0x00` with a final dispatch-table pointer
5. call a common post-registration helper
   - `0x005A7266`

So the thunks are not:

- passive data tables
- or ordinary runtime helper calls

They are:

- tiny registration functions that materialize installed callback records

## `ImgFlip` Installed Records

The clearest currently recovered `ImgFlip` installs are:

| thunk | constructor | key triple | destination object | patched dispatch pointer |
| --- | --- | --- | --- | --- |
| `0x0045D6E0` | `FUN_0068F540(...)` | `(0x8, 0x0, 0x1)` | `0x00BDB458` | `0x00A276F8` |
| `0x0045D710` | `FUN_0068F540(...)` | `(0x10, 0x0, 0x1)` | `0x00BDB47C` | `0x00A276FC` |
| `0x0045D740` | `FUN_0068F540(...)` | `(0x18, 0x0, 0x1)` | `0x00BDB4A0` | `0x00A27700` |

That is already enough to state the important part:

- the static installer is creating individual installed callback records
- not just seeding one monolithic family object

## `ImgFlip` Shared Method Bank

The patched `ImgFlip` dispatch pointers are not isolated per-record vtables.

They land in one contiguous method bank:

- `0x00A276F8 -> FUN_0068F130`
- `0x00A276FC -> FUN_0068F1B0`
- `0x00A27700 -> FUN_0068F230`
- `0x00A27704 -> FUN_0068F2B0`
- `0x00A27708 -> FUN_0068F330`
- `0x00A2770C -> FUN_0068F3A0`
- `0x00A27710 -> FUN_0068F420`
- `0x00A27714 -> FUN_0068F4D0`

Immediately after that bank is the source-path string:

- `0x00A27718`
  - `P:\\Code\\Engine\\Gr\\Img\\ImgFlip.cpp`

So the `ImgFlip` installer is clearly wiring records onto slices of a shared `ImgFlip.cpp` method array.

## Broad Transfer Installed Records

The broad transfer side follows the same compiled style.

Current installed examples already recovered from the static thunks are:

| key triple | patched dispatch pointer |
| --- | --- |
| `(0x11, 0x11, 0x1)` | `0x00A26C14` |
| `(0x11, 0x11, 0x3)` | `0x00A26C18` |
| `(0x12, 0x12, 0x1)` | `0x00A26C1C` |
| `(0x12, 0x12, 0x3)` | `0x00A26C20` |
| `(0x13, 0x13, 0x1)` | `0x00A26C24` |
| `(0x13, 0x13, 0x3)` | `0x00A26C28` |
| `(0x13, 0x13, 0x5)` | `0x00A26C2C` |
| `(0x14, 0x14, 0x1)` | `0x00A26C30` |
| `(0x15, 0x15, 0x1)` | `0x00A26C34` |
| `(0x1, 0x0, 0x1)` | `0x00A26C38` |

That already shows something important:

- the third key field is selecting sibling variants inside one format-family slice bank
- not a totally separate subsystem

## Broad Transfer Method Bank

The patched broad-transfer pointers also land in a contiguous shared bank:

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
- continuing through:
  - `0x00A26C70 -> FUN_0067A6B0`

So the broad transfer registry is also not pointing at one-off object tables.
It is pointing into a prebuilt shared kernel bank.

## Why This Matters

This changes the callback picture in a useful way.

Before this step, the best statement was:

- records are statically installed and patched to final dispatch pointers

Now the stronger statement is:

- the final dispatch pointers are often slices into shared callback-method banks
- and those bank slices already map to concrete family workers

That means the registry system is even more table-driven than it first looked:

- generic record creation
- static key-based installation
- slice selection inside shared method banks

## Bridge to the Transfer-Bank Taxonomy

The broad transfer bank is already semantically tied back to the image/compressed-family work.

Its early slots are not lightweight policy callbacks.
They are real worker kernels:

- `FUN_006743A0(...)`
- `FUN_00674B20(...)`
- `FUN_00675260(...)`

and these already show:

- RGB565 expansion
- familiar DXT-family interpolation tables
- sentinel-block handling
- and final repack through `FUN_006A5CB0(...)`

So the static thunks are doing something stronger than:

- install a generic callback object

They are:

- wiring key triples directly onto real format-family worker slices

That is the strongest installation-side bridge yet between:

- registry keys
- installed records
- and the already recovered transfer-bank / compressed-family workers

## Best Current Model

The callback-installation architecture now reads as:

1. static thunk pushes a key triple
2. generic constructor/inserter builds a base registry record
3. base record is inserted into the hashed registry
4. thunk patches the record onto a final dispatch-bank slice
5. lookup later recovers that record by key
6. runtime dispatch lands directly in a shared family worker bank

That is much sharper than the older “callback registry” description because it explains:

- where installed records come from
- how they are keyed
- how they get their final behavior
- and why the dispatch pointers cluster in contiguous ranges

## Practical Classifier

When reading a new static thunk in these regions, the fastest current classifier is:

1. if it calls `FUN_0068F540(...)`
   - treat it as `ImgFlip` registry installation
2. if it calls `FUN_00688720(...)`
   - treat it as broad transfer-registry installation
3. if it then patches `record + 0x00` to an address inside a contiguous bank
   - treat that address as the real runtime family slice
4. if the patched bank sits near known DXT-family workers
   - treat the thunk’s key triple as a concrete format-family binding

## Best Next Step

The strongest next reverse step is now:

- enumerate more static thunks in the broad transfer run
- then line their key triples up against the already recovered bank slices

The highest-value comparison is:

- the third-key variants `1 / 3 / 5`

because those now look very likely to encode concrete sibling modes inside one format family rather than different registries.

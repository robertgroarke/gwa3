# GWCA Toolbox VTable Boundary Addendum

## Scope

This pass follows the subobject-correction thread from:

- [GWCA_Toolbox_Subobject_Correction_Addendum.md](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\GWCA_Toolbox_Subobject_Correction_Addendum.md)

The immediate question was:

- who actually calls `0x100172B0`?

## Strongest result: `0x100172B0` is vtable-bound, not normally called by direct code references

Using the toolbox project’s xref pass for:

- `0x100172B0`

I only recovered two references:

1. a data reference at `0x10080184`
2. the export entry point itself

Artifact:

- [findcallers_100172b0.log](c:\Users\Robert\Documents\GWA%20Censured%20X%20BotsHub\tools\ghidra_projects\findcallers_100172b0.log)

That matters because it explains the earlier contradiction:

- we were looking for a normal direct call target
- but this address behaves like a method reached through data/vtable dispatch instead

So the lack of normal code xrefs is actually evidence, not failure.

## What this means for the earlier `0x100172B0` contradiction

This fits the previous correction well:

- the symbol name attached by Ghidra may still reflect one exported/typed interpretation
- but the actual body is being reached through vtable-style dispatch in other contexts
- therefore one label is not enough to describe all uses of the body

This makes the earlier “callable holder / erased callback” interpretation more plausible, not less.

If the record payload uses a virtual interface internally, then:

- direct static callers may be absent
- data/vtable references become the primary anchor

## Consequence for the next reverse step

The next logical step is no longer:

- “find direct callers of `0x100172B0`”

because this pass shows that path is mostly exhausted.

The better next step is:

1. inspect the data/vtable object at or around `0x10080184`
2. map which virtual table entry points include:
   - `0x10017290`
   - `0x100172B0`
   - `0x100172E0`
   - `0x10017300`
   - `0x10017340`
3. determine whether the hidden listener payload at `+0x08` embeds one of those vtable-bearing UI/control-style helper objects or a reusable callable wrapper that shares the same table layout

## Secondary result: the listener-vector grow helper is still worth following

I also attempted to pivot to:

- `0x10003410`

which is the grow/inserter helper used when the listener vector is out of space.

Because of project locking noise, I did not get a clean decompile of that helper in this pass.
So that path remains open and still useful.

But after the `0x100172B0` xref result, the **vtable data path** is the higher-value next move.

## Best current interpretation

After this pass, the safest statement is:

- `0x100172B0` is not a plain directly-called helper in this seam
- it is effectively part of a vtable-dispatched method family
- the listener payload at `+0x08` is therefore best treated as a polymorphic embedded object, not a flat helper blob

That is a cleaner and more defensible model than chasing nonexistent direct callers.

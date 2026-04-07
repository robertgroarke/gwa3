import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.ReferenceManager;

public class FindCallers extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            println("Usage: FindCallers <target_hex>");
            return;
        }
        String raw = args[0].trim();
        if (raw.startsWith("0x") || raw.startsWith("0X")) raw = raw.substring(2);
        Address target = toAddr(Long.parseUnsignedLong(raw, 16));
        if (target == null) {
            println("Invalid address");
            return;
        }

        ReferenceManager rm = currentProgram.getReferenceManager();
        ReferenceIterator refs = rm.getReferencesTo(target);
        int count = 0;
        while (refs.hasNext()) {
            Reference ref = refs.next();
            Address from = ref.getFromAddress();
            Function f = getFunctionContaining(from);
            println(String.format("REF from %s in %s", from, f != null ? f.getName() + " @ " + f.getEntryPoint() : "<no function>"));
            count++;
        }
        println("TOTAL_REFS=" + count);
    }
}

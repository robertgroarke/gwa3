import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.CodeUnit;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class FindStringXrefs extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            println("Usage: FindStringXrefs <substring>");
            return;
        }

        String needle = args[0];
        SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
        int hits = 0;
        while (it.hasNext()) {
            Symbol sym = it.next();
            Address addr = sym.getAddress();
            Data data = getDataAt(addr);
            if (data == null) {
                continue;
            }
            Object value = data.getValue();
            if (!(value instanceof String)) {
                continue;
            }
            String s = (String) value;
            if (!s.contains(needle)) {
                continue;
            }
            hits++;
            println("STRING=" + addr + " VALUE=" + s);
            Reference[] refs = getReferencesTo(addr);
            for (Reference ref : refs) {
                Address from = ref.getFromAddress();
                CodeUnit cu = currentProgram.getListing().getCodeUnitAt(from);
                Function fn = getFunctionContaining(from);
                String fnLabel = fn == null ? "<no function>" : (fn.getName() + " @ " + fn.getEntryPoint());
                println("  XREF_FROM=" + from + " FUNC=" + fnLabel + " TEXT=" + (cu == null ? "<no codeunit>" : cu.toString()));
            }
            println("----");
        }
        println("MATCHES=" + hits);
    }
}

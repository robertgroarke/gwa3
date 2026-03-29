import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class ListFunctionsRange extends GhidraScript {
    @Override
    public void run() throws Exception {
        if (currentProgram == null) {
            println("No program loaded");
            return;
        }
        if (getScriptArgs().length < 2) {
            println("Usage: ListFunctionsRange <start_hex> <end_hex>");
            return;
        }

        String startRaw = getScriptArgs()[0].trim();
        String endRaw = getScriptArgs()[1].trim();
        if (startRaw.startsWith("0x") || startRaw.startsWith("0X")) startRaw = startRaw.substring(2);
        if (endRaw.startsWith("0x") || endRaw.startsWith("0X")) endRaw = endRaw.substring(2);

        Address start = toAddr(Long.parseUnsignedLong(startRaw, 16));
        Address end = toAddr(Long.parseUnsignedLong(endRaw, 16));
        if (start == null || end == null) {
            println("Invalid address");
            return;
        }

        FunctionIterator funcs = currentProgram.getFunctionManager().getFunctions(start, true);
        int count = 0;
        while (funcs.hasNext()) {
            Function f = funcs.next();
            if (f.getEntryPoint().compareTo(end) > 0) break;
            println(f.getName() + " @ " + f.getEntryPoint());
            count++;
        }
        println("TOTAL_LISTED=" + count);
    }
}

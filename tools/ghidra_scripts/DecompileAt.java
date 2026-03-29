// Decompile function containing an address passed as hex, e.g. 10016660 or 0x10016660
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class DecompileAt extends GhidraScript {
    @Override
    protected void run() throws Exception {
        if (currentProgram == null) {
            println("No program loaded");
            return;
        }
        if (getScriptArgs().length < 1) {
            println("Usage: DecompileAt <hex_address>");
            return;
        }

        String raw = getScriptArgs()[0].trim();
        if (raw.startsWith("0x") || raw.startsWith("0X")) {
            raw = raw.substring(2);
        }

        Address addr = toAddr(Long.parseUnsignedLong(raw, 16));
        if (addr == null) {
            println("Invalid address");
            return;
        }

        Function func = getFunctionContaining(addr);
        if (func == null) {
            func = getFunctionAt(addr);
        }
        if (func == null) {
            println("No function found at/containing " + addr);
            return;
        }

        println("FUNCTION=" + func.getName() + " @ " + func.getEntryPoint());

        DecompInterface ifc = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        ifc.setOptions(options);
        ifc.openProgram(currentProgram);

        DecompileResults res = ifc.decompileFunction(func, 60, monitor);
        if (!res.decompileCompleted()) {
            println("Decompile failed: " + res.getErrorMessage());
            return;
        }

        println(res.getDecompiledFunction().getC());
    }
}

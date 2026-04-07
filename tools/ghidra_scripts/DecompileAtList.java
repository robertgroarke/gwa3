import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class DecompileAtList extends GhidraScript {
    @Override
    protected void run() throws Exception {
        if (currentProgram == null) {
            println("No program loaded");
            return;
        }
        String[] args = getScriptArgs();
        if (args.length < 1) {
            println("Usage: DecompileAtList <hex_address> [more_hex_addresses...]");
            return;
        }

        DecompInterface ifc = new DecompInterface();
        DecompileOptions options = new DecompileOptions();
        ifc.setOptions(options);
        ifc.openProgram(currentProgram);

        for (String rawArg : args) {
            String raw = rawArg.trim();
            if (raw.startsWith("0x") || raw.startsWith("0X")) {
                raw = raw.substring(2);
            }
            Address addr = toAddr(Long.parseUnsignedLong(raw, 16));
            if (addr == null) {
                println("Invalid address: " + rawArg);
                continue;
            }

            Function func = getFunctionContaining(addr);
            if (func == null) {
                func = getFunctionAt(addr);
            }
            if (func == null) {
                println("NO_FUNCTION_AT=" + addr);
                continue;
            }

            println("FUNCTION=" + func.getName() + " @ " + func.getEntryPoint() + " FOR_ADDR=" + addr);
            DecompileResults res = ifc.decompileFunction(func, 60, monitor);
            if (!res.decompileCompleted()) {
                println("DECOMPILE_FAILED_FOR=" + addr + " ERR=" + res.getErrorMessage());
                continue;
            }
            println(res.getDecompiledFunction().getC());
            println("====");
        }
    }
}

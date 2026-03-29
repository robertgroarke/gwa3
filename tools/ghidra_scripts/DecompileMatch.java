import java.util.ArrayList;
import java.util.List;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class DecompileMatch extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length == 0 || args[0].isEmpty()) {
            println("NO_TARGET");
            return;
        }
        String target = args[0].toLowerCase();

        List<Function> matches = new ArrayList<>();
        FunctionIterator funcs = currentProgram.getFunctionManager().getFunctions(true);
        while (funcs.hasNext()) {
            Function f = funcs.next();
            if (f.getName().toLowerCase().contains(target)) {
                matches.add(f);
            }
        }

        println("MATCHES=" + matches.size());
        for (int i = 0; i < matches.size() && i < 10; i++) {
            Function f = matches.get(i);
            println("MATCH " + f.getName() + " @ " + f.getEntryPoint());
        }
        if (matches.isEmpty()) {
            return;
        }

        Function func = matches.get(0);
        DecompInterface ifc = new DecompInterface();
        ifc.openProgram(currentProgram);
        DecompileResults res = ifc.decompileFunction(func, 60, monitor);
        if (!res.decompileCompleted()) {
            println("DECOMPILE_FAILED");
            return;
        }

        println("DECOMPILED_FUNCTION=" + func.getName());
        println(res.getDecompiledFunction().getC());
    }
}

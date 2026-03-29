import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class ListFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        int limit = Integer.MAX_VALUE;
        if (getScriptArgs().length >= 1) {
            limit = Integer.parseInt(getScriptArgs()[0]);
        }
        FunctionIterator funcs = currentProgram.getFunctionManager().getFunctions(true);
        int count = 0;
        while (funcs.hasNext() && count < limit) {
            Function f = funcs.next();
            println(f.getName() + " @ " + f.getEntryPoint());
            count++;
        }
        println("TOTAL_LISTED=" + count);
    }
}

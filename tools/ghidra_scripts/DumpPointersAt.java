import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.scalar.Scalar;

public class DumpPointersAt extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            println("Usage: DumpPointersAt <hex_address> [count]");
            return;
        }

        String raw = args[0].trim();
        if (raw.startsWith("0x") || raw.startsWith("0X")) {
            raw = raw.substring(2);
        }
        Address base = toAddr(Long.parseUnsignedLong(raw, 16));
        if (base == null) {
            println("Invalid address");
            return;
        }

        int count = args.length >= 2 ? Integer.parseInt(args[1]) : 8;
        Memory mem = currentProgram.getMemory();

        for (int i = 0; i < count; i++) {
            Address addr = base.add(i * 4L);
            int value;
            try {
                value = mem.getInt(addr);
            } catch (Exception e) {
                println(String.format("%s: <read failed> %s", addr, e.getMessage()));
                continue;
            }

            long unsigned = Integer.toUnsignedLong(value);
            Address target = toAddr(unsigned);
            String targetStr = target == null ? String.format("0x%08X", value) : target.toString();

            Function func = target == null ? null : getFunctionAt(target);
            if (func == null && target != null) {
                func = getFunctionContaining(target);
            }

            Data data = getDataAt(addr);
            String dataStr = data == null ? "" : (" data=" + data.getDataType().getName());

            if (func != null) {
                println(String.format("%s -> %s (%s)%s", addr, targetStr, func.getName(), dataStr));
            } else {
                println(String.format("%s -> %s%s", addr, targetStr, dataStr));
            }
        }
    }
}

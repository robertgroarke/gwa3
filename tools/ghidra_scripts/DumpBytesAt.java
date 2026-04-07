import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;

public class DumpBytesAt extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            println("Usage: DumpBytesAt <hex_address> [count]");
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

        int count = args.length >= 2 ? Integer.parseInt(args[1]) : 16;
        Memory mem = currentProgram.getMemory();
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < count; i++) {
            Address addr = base.add(i);
            byte b;
            try {
                b = mem.getByte(addr);
            } catch (Exception e) {
                println(String.format("%s: <read failed> %s", addr, e.getMessage()));
                return;
            }
            sb.append(String.format("%s: %02X%n", addr, b & 0xff));
        }
        println(sb.toString());
    }
}

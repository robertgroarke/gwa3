import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import java.util.Arrays;

public class DisassembleAround extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            println("Usage: DisassembleAround <hex_address> [before_count] [after_count]");
            return;
        }

        String raw = args[0].trim();
        if (raw.startsWith("0x") || raw.startsWith("0X")) {
            raw = raw.substring(2);
        }
        Address addr = toAddr(Long.parseUnsignedLong(raw, 16));
        if (addr == null) {
            println("Invalid address");
            return;
        }

        int before = args.length >= 2 ? Integer.parseInt(args[1]) : 8;
        int after = args.length >= 3 ? Integer.parseInt(args[2]) : 12;

        Instruction center = getInstructionContaining(addr);
        if (center == null) {
            center = getInstructionAt(addr);
        }
        if (center == null) {
            println("No instruction at/containing " + addr);
            return;
        }

        Instruction start = center;
        for (int i = 0; i < before; i++) {
            Instruction prev = start.getPrevious();
            if (prev == null) break;
            start = prev;
        }

        Instruction cur = start;
        int total = before + after + 1;
        for (int i = 0; i < total && cur != null; i++) {
            String marker = cur.getMinAddress().compareTo(addr) <= 0 && cur.getMaxAddress().compareTo(addr) >= 0 ? ">>" : "  ";
            byte[] bytes = cur.getBytes();
            StringBuilder hex = new StringBuilder();
            for (int j = 0; j < bytes.length; j++) {
                if (j > 0) hex.append(' ');
                hex.append(String.format("%02X", bytes[j] & 0xff));
            }
            println(String.format("%s %s: %-24s ; %s", marker, cur.getAddress(), cur.toString(), hex.toString()));
            cur = cur.getNext();
        }
    }
}

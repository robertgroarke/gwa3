"""Fire scan_ui_labels via direct win32 pipe write, bypassing
BridgeTestCase's flaky async setUp. Read the resulting snapshot
to check for decoded quest names."""
import json, os, struct, sys, time
import win32file, pywintypes

PIPE = r"\\.\pipe\gwa3_llm_biscuit"

def connect(timeout=30.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            h = win32file.CreateFile(PIPE,
                win32file.GENERIC_READ | win32file.GENERIC_WRITE,
                0, None, win32file.OPEN_EXISTING, 0, None)
            return h
        except pywintypes.error:
            time.sleep(0.5)
    return None

def send_msg(h, payload):
    data = json.dumps(payload).encode("utf-8")
    # Our bridge protocol: 4-byte LE length prefix then JSON bytes.
    win32file.WriteFile(h, struct.pack("<I", len(data)) + data)

def read_msg(h, timeout=5.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            _, lenbuf = win32file.ReadFile(h, 4)
            if len(lenbuf) != 4:
                return None
            length = struct.unpack("<I", lenbuf)[0]
            _, databuf = win32file.ReadFile(h, length)
            return json.loads(databuf.decode("utf-8"))
        except pywintypes.error:
            return None
    return None

def main():
    h = connect()
    if not h:
        print("failed to connect")
        return
    print("connected")
    # Fire the scan
    send_msg(h, {"type": "action", "name": "scan_ui_labels", "params": {},
                 "request_id": "scan1"})
    # Drain messages for a few seconds looking for a tier-2 snapshot
    t2 = None
    start = time.monotonic()
    while time.monotonic() - start < 12.0:
        msg = read_msg(h, timeout=1.0)
        if not msg:
            continue
        mtype = msg.get("type")
        if mtype == "action_result" and msg.get("request_id") == "scan1":
            print(f"scan_ui_labels: {msg.get('success')} {msg.get('error')}")
        elif mtype == "snapshot" and msg.get("tier") == 2:
            t2 = msg
            # keep reading — we want the snapshot AFTER scan_ui_labels ran
            if time.monotonic() - start > 6.0:
                break
    win32file.CloseHandle(h)
    if not t2:
        print("no tier-2 snapshot received")
        return
    q = t2["quests"]
    aq = q.get("active_quest") or {}
    log = q.get("quest_log", [])
    print(f"\nactive_quest_id={q['active_quest_id']}  log_size={len(log)}")
    dec = {k: v for k, v in aq.items() if not k.endswith("_enc") and isinstance(v, str)}
    print(f"active_quest decoded fields: {list(dec.keys())}")
    for k, v in dec.items():
        print(f"  {k}: {(v[:160] + '...') if len(v) > 160 else v}")
    n_decoded = 0
    for e in log:
        for k in ("name", "location", "npc"):
            if k in e:
                n_decoded += 1
                print(f"  log quest_id={e['quest_id']} {k}: {e[k]}")
    print(f"\ntotal decoded fields in log: {n_decoded}")


if __name__ == "__main__":
    main()

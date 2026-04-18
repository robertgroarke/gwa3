"""E2E via direct pipe: open Quest Log, wait, scan, dump snapshot."""
import json, struct, sys, time
import win32file, pywintypes

PIPE = r"\\.\pipe\gwa3_llm_biscuit"

def connect(timeout=30.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            return win32file.CreateFile(PIPE,
                win32file.GENERIC_READ | win32file.GENERIC_WRITE,
                0, None, win32file.OPEN_EXISTING, 0, None)
        except pywintypes.error:
            time.sleep(0.5)
    return None

def send_msg(h, payload):
    data = json.dumps(payload).encode("utf-8")
    win32file.WriteFile(h, struct.pack("<I", len(data)) + data)

def read_msg(h):
    _, lenbuf = win32file.ReadFile(h, 4)
    if len(lenbuf) != 4: return None
    length = struct.unpack("<I", lenbuf)[0]
    _, databuf = win32file.ReadFile(h, length)
    return json.loads(databuf.decode("utf-8"))

def drain_until(h, predicate, seconds):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        try:
            msg = read_msg(h)
        except pywintypes.error:
            return None
        if msg and predicate(msg):
            return msg
    return None

def main():
    h = connect()
    if not h: print("no connect"); return
    print("connected")
    send_msg(h, {"type": "action", "name": "open_quest_log", "params": {},
                 "request_id": "open1"})
    r = drain_until(h, lambda m: m.get("type") == "action_result" and m.get("request_id") == "open1", 5.0)
    print(f"open: {r and r.get('success')}")
    time.sleep(4)
    send_msg(h, {"type": "action", "name": "scan_ui_labels", "params": {},
                 "request_id": "scan1"})
    r = drain_until(h, lambda m: m.get("type") == "action_result" and m.get("request_id") == "scan1", 5.0)
    print(f"scan: {r and r.get('success')}")
    time.sleep(3)
    snap = drain_until(h, lambda m: m.get("type") == "snapshot" and m.get("tier") == 2, 8.0)
    win32file.CloseHandle(h)
    if not snap: print("no tier-2 snap"); return
    q = snap["quests"]
    aq = q.get("active_quest") or {}
    log = q.get("quest_log", [])
    print(f"\nactive_quest_id={q['active_quest_id']}  log_size={len(log)}")
    dec_active = {k: v for k, v in aq.items() if not k.endswith("_enc") and isinstance(v, str)}
    print(f"active_quest decoded fields: {list(dec_active.keys())}")
    for k, v in dec_active.items():
        print(f"  {k}: {(v[:160] + '...') if len(v) > 160 else v}")
    ndec = 0
    for e in log:
        for k in ("name", "location", "npc"):
            if k in e:
                ndec += 1
                print(f"  log quest_id={e['quest_id']} {k}: {e[k]}")
    print(f"\ntotal decoded fields in log: {ndec}")


if __name__ == "__main__":
    main()

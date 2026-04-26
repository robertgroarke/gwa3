import asyncio
import os
import sys

# Use the real client
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from bridge.ipc_client import IpcClient

PIPE = r"\\.\pipe\gwa3_llm_biscuit"


async def main():
    c = IpcClient(PIPE)
    ok = await c.connect(timeout=10.0)
    print("connected:", ok)
    if not ok:
        return
    n_msgs = 0
    start = asyncio.get_event_loop().time()
    while asyncio.get_event_loop().time() - start < 8.0:
        try:
            msg = await asyncio.wait_for(c.read_message(), timeout=1.0)
            if msg is None:
                continue
            n_msgs += 1
            t = msg.get("type")
            tier = msg.get("tier")
            if n_msgs <= 8:
                print(f"  msg #{n_msgs}: type={t} tier={tier} keys={list(msg.keys())[:10]}")
        except asyncio.TimeoutError:
            print("  read timeout")
        except Exception as e:
            print(f"  exception: {type(e).__name__}: {e}")
            break
    print(f"total msgs: {n_msgs}")
    c.disconnect()


if __name__ == "__main__":
    asyncio.run(main())

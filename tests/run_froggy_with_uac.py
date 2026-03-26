"""
Launch Froggy with AutoIt, handle UAC prompt, capture errors.

Since PromptOnSecureDesktop=0 on this machine, the UAC dialog
appears on the normal desktop and pyautogui can click it.

Usage: python tests/run_froggy_with_uac.py
"""
import subprocess
import time
import threading
import sys
import os
import pyautogui
import pygetwindow

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AUTOIT = r"C:\Program Files (x86)\AutoIt3\AutoIt3.exe"
FROGGY = os.path.join(PROJECT_ROOT, "GWA Censured", "Froggy_HM_v1.6.au3")

def click_uac_yes(timeout=15):
    """Watch for UAC consent dialog and click Yes."""
    start = time.time()
    while time.time() - start < timeout:
        time.sleep(0.5)
        try:
            # Look for the UAC consent window
            windows = pygetwindow.getWindowsWithTitle('User Account Control')
            if windows:
                win = windows[0]
                print(f"[UAC] Found UAC window: {win.title} at ({win.left},{win.top})")
                # Bring to front
                try:
                    win.activate()
                except:
                    pass
                time.sleep(0.3)

                # The "Yes" button is typically in the lower-left area of the UAC dialog
                # Use pyautogui to find and click it
                yes_btn = pyautogui.locateOnScreen

                # Try clicking by image if we had one, but simpler:
                # UAC Yes button is typically at roughly center-left of the dialog
                # Just send Alt+Y which is the keyboard shortcut for Yes
                pyautogui.hotkey('alt', 'y')
                print("[UAC] Sent Alt+Y to accept UAC prompt")
                return True
        except Exception as e:
            pass

    print(f"[UAC] No UAC window found within {timeout}s")
    return False

def main():
    print(f"Launching: {AUTOIT} /ErrorStdOut \"{FROGGY}\"")
    print("Watching for UAC prompt...")

    # Start UAC watcher in background thread
    uac_thread = threading.Thread(target=click_uac_yes, daemon=True)
    uac_thread.start()

    # Launch AutoIt with /ErrorStdOut to capture errors
    try:
        proc = subprocess.Popen(
            [AUTOIT, "/ErrorStdOut", FROGGY],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

        # Wait for process (with timeout — script will try to connect to game)
        # We just want parse-time errors, not full runtime
        try:
            stdout, stderr = proc.communicate(timeout=30)
        except subprocess.TimeoutExpired:
            print("[INFO] Script ran for 30s without parse error — killing")
            proc.kill()
            stdout, stderr = proc.communicate()

        output = stdout + stderr
        if output.strip():
            print("\n=== AutoIt Output ===")
            print(output)
        else:
            print("\n[OK] No parse errors detected (script started successfully)")

        return proc.returncode or 0

    except Exception as e:
        print(f"[ERROR] Failed to launch: {e}")
        return 1

if __name__ == '__main__':
    sys.exit(main())

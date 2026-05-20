"""Public-safe launcher/injector adapter for the bridge HTTP API.

The public repository does not ship account-specific launcher scripts or
character names. Configure those locally with environment variables before
using ``POST /api/llm/launch``:

- ``GWA3_LAUNCHER_SCRIPT``: absolute path to the local launcher script.
- ``GWA3_AUTOIT_EXE``: AutoIt executable path, if needed.
- ``GWA3_BUILD_DIR``: build directory containing ``bin/Release``.
- ``GWA3_DLL_NAME``: DLL name to inject, default ``gwa3.dll``.
- ``GWA3_LLM_LANE``: lane name accepted by the HTTP API, default ``default``.
"""

from __future__ import annotations

import asyncio
import os
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass
class LaunchResult:
    ok: bool
    status: str
    lane: str
    pid: int | None = None
    injected: bool = False
    error: str | None = None
    message: str | None = None

    def to_dict(self) -> dict[str, Any]:
        return {
            "ok": self.ok,
            "status": self.status,
            "lane": self.lane,
            "pid": self.pid,
            "injected": self.injected,
            "error": self.error,
            "message": self.message,
        }


class PublicLaneLauncher:
    """Launches a locally configured public-safe lane and injects its PID."""

    def __init__(self, repo_root: Path):
        self.repo_root = repo_root
        self.lane = (os.environ.get("GWA3_LLM_LANE") or "default").strip().lower()
        self.build_dir = Path(os.environ.get("GWA3_BUILD_DIR") or repo_root / "build")
        self.dll_name = os.environ.get("GWA3_DLL_NAME") or "gwa3.dll"
        self.injector_exe = self.build_dir / "bin" / "Release" / "injector.exe"
        script = os.environ.get("GWA3_LAUNCHER_SCRIPT")
        self.launcher_script = Path(script) if script else None
        autoit = os.environ.get("GWA3_AUTOIT_EXE") or r"C:\Program Files (x86)\AutoIt3\AutoIt3.exe"
        self.autoit_exe = Path(autoit)
        self.started_pid: int | None = None

    async def launch_and_inject(self) -> dict[str, Any]:
        if self.launcher_script is None:
            return LaunchResult(
                ok=False,
                status="launch_unavailable",
                lane=self.lane,
                error="launcher_not_configured",
                message="Set GWA3_LAUNCHER_SCRIPT to enable public-repo HTTP launch.",
            ).to_dict()
        try:
            self._preflight()
            pid = await self._launch()
            self.started_pid = pid
            return await self._inject(pid)
        except Exception as exc:
            return LaunchResult(
                ok=False,
                status="launch_error",
                lane=self.lane,
                error=type(exc).__name__,
                message=str(exc),
            ).to_dict()

    async def stop_started_client(self) -> dict[str, Any]:
        if self.started_pid is None:
            return {"ok": True, "lane": self.lane, "stopped": False, "message": "no bridge-owned pid"}
        try:
            proc = await asyncio.create_subprocess_exec(
                "taskkill",
                "/PID",
                str(self.started_pid),
                "/T",
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
            )
            stdout, stderr = await proc.communicate()
            pid = self.started_pid
            self.started_pid = None
            return {
                "ok": proc.returncode == 0,
                "lane": self.lane,
                "stopped": proc.returncode == 0,
                "pid": pid,
                "stdout": stdout.decode(errors="replace"),
                "stderr": stderr.decode(errors="replace"),
            }
        except Exception as exc:
            return {"ok": False, "lane": self.lane, "error": type(exc).__name__, "message": str(exc)}

    def _preflight(self) -> None:
        if not self.autoit_exe.exists():
            raise FileNotFoundError(str(self.autoit_exe))
        if self.launcher_script is None or not self.launcher_script.exists():
            raise FileNotFoundError(str(self.launcher_script))
        if not self.injector_exe.exists():
            raise FileNotFoundError(str(self.injector_exe))
        dll_path = self.injector_exe.parent / self.dll_name
        if not dll_path.exists():
            raise FileNotFoundError(str(dll_path))

    async def _launch(self) -> int:
        assert self.launcher_script is not None
        proc = await asyncio.create_subprocess_exec(
            str(self.autoit_exe),
            str(self.launcher_script),
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            cwd=str(self.repo_root),
        )
        stdout, stderr = await proc.communicate()
        text = "\n".join(
            part.decode(errors="replace")
            for part in (stdout, stderr)
            if part
        )
        if proc.returncode != 0:
            raise RuntimeError(f"launcher exited {proc.returncode}: {text[-1000:]}")
        match = re.search(r"GWLAUNCHER_PID\s*=\s*(\d+)|pid\s*[:=]\s*(\d+)", text, re.IGNORECASE)
        if not match:
            raise RuntimeError("launcher did not print a PID")
        return int(match.group(1) or match.group(2))

    async def _inject(self, pid: int) -> dict[str, Any]:
        proc = await asyncio.create_subprocess_exec(
            str(self.injector_exe),
            "--pid",
            str(pid),
            "--dll",
            self.dll_name,
            "--llm",
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            cwd=str(self.injector_exe.parent),
        )
        stdout, stderr = await proc.communicate()
        if proc.returncode != 0:
            return LaunchResult(
                ok=False,
                status="inject_error",
                lane=self.lane,
                pid=pid,
                error="injector_failed",
                message=(stdout + stderr).decode(errors="replace")[-1000:],
            ).to_dict()
        return LaunchResult(ok=True, status="injected", lane=self.lane, pid=pid, injected=True).to_dict()


BridgeLaneLauncher = PublicLaneLauncher

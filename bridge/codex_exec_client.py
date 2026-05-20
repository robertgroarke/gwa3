"""Codex CLI backed LLM client.

This provider is intentionally small and opt-in. Codex research-preview models
such as GPT-5.3-Codex-Spark are available through the Codex CLI rather than the
local OpenAI-compatible HTTP endpoint used by Ollama/vLLM. The bridge still
expects an LLMResponse with tool calls, so this module asks `codex exec` to emit
that same shape as strict JSON and then adapts it into the normal client model.
"""

import asyncio
import json
import os
import shutil
import tempfile
import uuid
from pathlib import Path
from typing import Any

from .llm_client import LLMResponse, ToolCall


CODEX_EXEC_RESPONSE_SCHEMA = {
    "type": "object",
    "additionalProperties": False,
    "required": ["content", "tool_calls"],
    "properties": {
        "content": {"type": ["string", "null"]},
        "tool_calls": {
            "type": "array",
            "maxItems": 3,
            "items": {
                "type": "object",
                "additionalProperties": False,
                "required": ["name", "arguments"],
                "properties": {
                    "id": {"type": ["string", "null"]},
                    "name": {"type": "string"},
                    "arguments": {
                        "type": "object",
                        "additionalProperties": True,
                    },
                },
            },
        },
    },
}


class CodexExecLLMClient:
    """Async LLM client that shells out to `codex exec` for each decision."""

    def __init__(self, model: str, timeout: float = 180.0, workdir: str | None = None):
        self.model = normalize_codex_model_name(model)
        self.timeout = timeout
        self.workdir = workdir or os.getcwd()

    async def chat_completion(
        self,
        messages: list[dict],
        tools: list[dict] | None = None,
        tool_choice: str = "auto",
        temperature: float = 0.3,
        max_tokens: int = 2048,
    ) -> LLMResponse:
        """Call Codex non-interactively and parse the final JSON response."""
        del tool_choice, temperature, max_tokens

        output_path = self._make_temp_path("json")
        schema_path = self._make_temp_path("schema.json")
        prompt = self._build_prompt(messages, tools or [])
        try:
            schema_path.write_text(
                json.dumps(CODEX_EXEC_RESPONSE_SCHEMA, separators=(",", ":")),
                encoding="utf-8",
            )
            proc = await asyncio.create_subprocess_exec(
                _codex_executable(),
                "exec",
                "-m",
                self.model,
                "--ephemeral",
                "--sandbox",
                "read-only",
                "--skip-git-repo-check",
                "--ignore-rules",
                "--ignore-user-config",
                "-c",
                'approval_policy="never"',
                "--output-schema",
                str(schema_path),
                "-o",
                str(output_path),
                "-",
                cwd=self.workdir,
                stdin=asyncio.subprocess.PIPE,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
            )
            try:
                stdout, stderr = await asyncio.wait_for(
                    proc.communicate(input=prompt.encode("utf-8")),
                    timeout=self.timeout,
                )
            except asyncio.TimeoutError:
                proc.kill()
                await proc.communicate()
                raise TimeoutError(f"codex exec timed out after {self.timeout:.0f}s")

            if proc.returncode != 0:
                detail = format_codex_exec_failure(stdout, stderr)
                raise RuntimeError(f"codex exec failed with exit {proc.returncode}: {detail}")

            text = output_path.read_text(encoding="utf-8", errors="replace")
            return parse_codex_exec_response(text, allowed_tools=_tool_names(tools or []))
        finally:
            try:
                output_path.unlink(missing_ok=True)
                schema_path.unlink(missing_ok=True)
            except OSError:
                pass

    async def close(self):
        """Match the HTTP client interface."""
        return None

    @staticmethod
    def _make_temp_path(suffix: str) -> Path:
        name = f"gwa3_codex_exec_{uuid.uuid4().hex}.{suffix}"
        return Path(tempfile.gettempdir()) / name

    def _build_prompt(self, messages: list[dict], tools: list[dict]) -> str:
        compact_tools = []
        for tool in tools:
            fn = tool.get("function", {}) if isinstance(tool, dict) else {}
            if not fn.get("name"):
                continue
            compact_tools.append({
                "name": fn.get("name"),
                "description": fn.get("description", ""),
                "parameters": fn.get("parameters", {}),
            })

        payload = {
            "messages": messages,
            "tools": compact_tools,
        }
        return (
            "You are the decision model for the GWA3 Guild Wars bridge.\n"
            "Choose the next action from the provided tools. Return ONLY a JSON "
            "object with this exact schema and no markdown:\n"
            "{"
            '"content": string|null, '
            '"tool_calls": ['
            '{"id": string|null, "name": string, "arguments": object}'
            "]"
            "}\n"
            "Use at most one tool call unless the state explicitly requires a "
            "small batch. If no action is appropriate, return an empty tool_calls "
            "array and a short content reason.\n\n"
            f"{json.dumps(payload, ensure_ascii=True, separators=(',', ':'))}"
        )


def parse_codex_exec_response(text: str, allowed_tools: set[str] | None = None) -> LLMResponse:
    """Parse the strict JSON object requested from Codex exec."""
    raw = _strip_markdown_fence(text.strip())
    try:
        data = json.loads(raw)
    except json.JSONDecodeError:
        data = json.loads(_extract_json_object(raw))

    result = LLMResponse(
        content=data.get("content"),
        finish_reason="stop",
        usage={},
    )
    allowed = allowed_tools or set()

    for index, raw_call in enumerate(data.get("tool_calls") or []):
        name, arguments = _normalize_tool_call(raw_call)
        if not name:
            continue
        if allowed and name not in allowed:
            result.content = _append_content(
                result.content,
                f"Skipped unsupported tool from codex-exec provider: {name}",
            )
            continue
        result.tool_calls.append(
            ToolCall(
                id=str(raw_call.get("id") or f"codex_call_{index + 1}"),
                name=name,
                arguments=arguments,
            )
        )

    return result


def format_codex_exec_failure(stdout: bytes, stderr: bytes) -> str:
    """Keep actionable Codex CLI errors ahead of noisy plugin/skill warnings."""
    text = b"\n".join(part for part in (stderr, stdout) if part).decode(
        "utf-8",
        errors="replace",
    )
    if not text:
        return "no output"

    lines = [line.strip() for line in text.splitlines() if line.strip()]
    error_lines = [
        line for line in lines
        if line.startswith("ERROR:") or "usage limit" in line.lower()
    ]
    if error_lines:
        return "\n".join(dict.fromkeys(error_lines))[-4000:]
    return text[-4000:]


def normalize_codex_model_name(model: str) -> str:
    """Map architecture model names onto Codex CLI model ids."""
    aliases = {
        "gpt-5.3-spark": "gpt-5.3-codex-spark",
        "gpt-5.3-codex-spark": "gpt-5.3-codex-spark",
    }
    return aliases.get((model or "").strip().lower(), model)


async def check_codex_exec_available(model: str, timeout: float = 30.0) -> dict[str, Any]:
    """Validate that `codex exec` can make a tiny call with the requested model."""
    login = await _codex_login_status(timeout=min(timeout, 10.0))
    if not login.get("ok"):
        return {
            "ok": False,
            "error": "codex_auth_failed",
            "model": model,
            "message": login.get("message", "codex login status failed"),
        }

    client = CodexExecLLMClient(model, timeout=timeout)
    try:
        await client.chat_completion(
            messages=[{
                "role": "user",
                "content": "Return {\"content\":\"ok\",\"tool_calls\":[]} exactly.",
            }],
            tools=[],
        )
    except Exception as exc:
        text = str(exc)
        lowered = text.lower()
        if "usage limit" in lowered:
            error = "codex_usage_limited"
        elif "timed out" in lowered:
            error = "codex_timeout"
        elif "unauthorized" in lowered or "not logged in" in lowered or "401" in lowered:
            error = "codex_auth_failed"
        else:
            error = "codex_exec_failed"
        return {
            "ok": False,
            "error": error,
            "model": model,
            "message": text[-1000:],
        }
    finally:
        await client.close()

    return {"ok": True, "model": model}


async def _codex_login_status(timeout: float = 10.0) -> dict[str, Any]:
    try:
        proc = await asyncio.create_subprocess_exec(
            _codex_executable(),
            "login",
            "status",
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        stdout, stderr = await asyncio.wait_for(proc.communicate(), timeout=timeout)
    except Exception as exc:
        return {"ok": False, "message": str(exc)}

    text = b"\n".join(part for part in (stdout, stderr) if part).decode(
        "utf-8",
        errors="replace",
    ).strip()
    if proc.returncode == 0:
        return {"ok": True, "message": text}
    return {"ok": False, "message": text or f"codex login status exited {proc.returncode}"}


def _normalize_tool_call(raw_call: dict[str, Any]) -> tuple[str, str]:
    if "function" in raw_call and isinstance(raw_call["function"], dict):
        fn = raw_call["function"]
        name = str(fn.get("name") or "")
        args = fn.get("arguments", {})
    else:
        name = str(raw_call.get("name") or "")
        args = raw_call.get("arguments", {})

    if isinstance(args, str):
        # Validate and compact so downstream parsed_arguments still behaves.
        args_obj = json.loads(args or "{}")
    elif isinstance(args, dict):
        args_obj = args
    else:
        args_obj = {}
    return name, json.dumps(args_obj, separators=(",", ":"))


def _tool_names(tools: list[dict]) -> set[str]:
    names = set()
    for tool in tools:
        fn = tool.get("function", {}) if isinstance(tool, dict) else {}
        name = fn.get("name")
        if name:
            names.add(str(name))
    return names


def _codex_executable() -> str:
    return shutil.which("codex.cmd") or shutil.which("codex") or "codex"


def _strip_markdown_fence(text: str) -> str:
    if text.startswith("```"):
        lines = text.splitlines()
        if lines and lines[0].startswith("```"):
            lines = lines[1:]
        if lines and lines[-1].strip() == "```":
            lines = lines[:-1]
        return "\n".join(lines).strip()
    return text


def _extract_json_object(text: str) -> str:
    start = text.find("{")
    end = text.rfind("}")
    if start < 0 or end <= start:
        raise json.JSONDecodeError("No JSON object found", text, 0)
    return text[start:end + 1]


def _append_content(current: str | None, extra: str) -> str:
    if current:
        return f"{current}\n{extra}"
    return extra

"""OpenAI-compatible async HTTP client for local LLM inference (vLLM/Ollama)."""

import asyncio
import json
import os
from dataclasses import dataclass, field
from typing import Any

import httpx


@dataclass
class ToolCall:
    """A single tool/function call from the LLM response."""
    id: str
    name: str
    arguments: str  # JSON string

    @property
    def parsed_arguments(self) -> dict:
        """Parse JSON arguments string. Raises on malformed LLM output."""
        return json.loads(self.arguments)


@dataclass
class LLMResponse:
    """Parsed LLM chat completion response."""
    content: str | None = None
    tool_calls: list[ToolCall] = field(default_factory=list)
    finish_reason: str = ""
    usage: dict = field(default_factory=dict)


@dataclass
class LLMRoleClients:
    """Planner/executor clients for two-model mode."""
    planner: Any
    executor: Any

    async def close(self):
        await self.planner.close()
        await self.executor.close()


class LLMClient:
    """Async client for OpenAI-compatible chat completion API."""

    RETRYABLE_STATUS_CODES = {429, 500, 502, 503, 504}

    def __init__(self, base_url: str, model: str, timeout: float = 120.0):
        self.base_url = base_url.rstrip("/")
        self.model = model
        headers = {}
        api_key = os.environ.get("GWA3_OPENAI_API_KEY") or os.environ.get("OPENAI_API_KEY")
        if api_key:
            headers["Authorization"] = f"Bearer {api_key}"
        self._client = httpx.AsyncClient(timeout=timeout, headers=headers)

    async def chat_completion(
        self,
        messages: list[dict],
        tools: list[dict] | None = None,
        tool_choice: str = "auto",
        temperature: float = 0.3,
        max_tokens: int = 2048,
    ) -> LLMResponse:
        """Call the chat completions endpoint with optional tool use."""
        payload: dict[str, Any] = {
            "model": self.model,
            "messages": messages,
            "temperature": temperature,
            "max_tokens": max_tokens,
        }
        if tools:
            payload["tools"] = tools
            payload["tool_choice"] = tool_choice

        url = f"{self.base_url}/chat/completions"
        resp = await self._post_with_retries(url, payload)
        resp.raise_for_status()
        data = resp.json()

        choice = data["choices"][0]
        msg = choice["message"]

        result = LLMResponse(
            content=msg.get("content"),
            finish_reason=choice.get("finish_reason", ""),
            usage=data.get("usage", {}),
        )

        # Parse tool calls if present
        for tc in msg.get("tool_calls", []):
            result.tool_calls.append(
                ToolCall(
                    id=tc.get("id", ""),
                    name=tc["function"]["name"],
                    arguments=tc["function"].get("arguments", "{}"),
                )
            )

        return result

    async def _post_with_retries(self, url: str, payload: dict[str, Any]) -> httpx.Response:
        last_response: httpx.Response | None = None
        for attempt in range(4):
            try:
                resp = await self._client.post(url, json=payload)
                if resp.status_code not in self.RETRYABLE_STATUS_CODES:
                    return resp
                last_response = resp
            except (httpx.ConnectError, httpx.ReadTimeout, httpx.RemoteProtocolError):
                if attempt == 3:
                    raise
            await asyncio.sleep(2.0 * (attempt + 1))

        if last_response is not None:
            return last_response
        return await self._client.post(url, json=payload)

    async def list_models(self) -> set[str]:
        """Return model ids advertised by the OpenAI-compatible endpoint."""
        resp = await self._client.get(f"{self.base_url}/models")
        resp.raise_for_status()
        data = resp.json()
        models: set[str] = set()
        for item in data.get("data", []):
            model_id = item.get("id")
            if isinstance(model_id, str):
                models.add(model_id)
        return models

    async def close(self):
        await self._client.aclose()


async def check_models_available(
    base_url: str,
    required_models: list[str],
    timeout: float = 5.0,
) -> dict[str, Any]:
    """Validate that an OpenAI-compatible endpoint can serve required models."""
    client = LLMClient(base_url, "__model_preflight__", timeout=timeout)
    try:
        available = await client.list_models()
    except httpx.HTTPStatusError as exc:
        status_code = exc.response.status_code
        if status_code in {401, 403}:
            return {
                "ok": False,
                "error": "auth_failed",
                "message": (
                    "LLM endpoint rejected authentication. Set OPENAI_API_KEY "
                    "or GWA3_OPENAI_API_KEY for API-backed endpoints."
                ),
                "base_url": base_url,
                "required_models": required_models,
                "status_code": status_code,
            }
        return {
            "ok": False,
            "error": "model_list_failed",
            "message": str(exc),
            "base_url": base_url,
            "required_models": required_models,
            "status_code": status_code,
        }
    except httpx.HTTPError as exc:
        return {
            "ok": False,
            "error": "model_list_failed",
            "message": str(exc),
            "base_url": base_url,
            "required_models": required_models,
        }
    finally:
        await client.close()

    missing = [model for model in required_models if model not in available]
    if missing:
        return {
            "ok": False,
            "error": "models_unavailable",
            "message": "Configured LLM endpoint does not advertise required planner/executor models.",
            "base_url": base_url,
            "required_models": required_models,
            "missing_models": missing,
            "available_models": sorted(available),
        }
    return {
        "ok": True,
        "base_url": base_url,
        "required_models": required_models,
    }


def create_role_clients(
    base_url: str,
    planner_model: str,
    executor_model: str,
    planner_timeout: float = 10.0,
    executor_timeout: float = 2.0,
) -> LLMRoleClients:
    """Create independent OpenAI-compatible planner and executor clients."""
    return LLMRoleClients(
        planner=LLMClient(base_url, planner_model, timeout=planner_timeout),
        executor=LLMClient(base_url, executor_model, timeout=executor_timeout),
    )

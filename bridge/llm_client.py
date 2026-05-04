"""OpenAI-compatible async HTTP client for local LLM inference (vLLM/Ollama)."""

import json
import asyncio
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


class LLMClient:
    """Async client for OpenAI-compatible chat completion API."""

    RETRYABLE_STATUS_CODES = {429, 500, 502, 503, 504}

    def __init__(self, base_url: str, model: str):
        self.base_url = base_url.rstrip("/")
        self.model = model
        self._client = httpx.AsyncClient(timeout=300.0)

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

    async def close(self):
        await self._client.aclose()

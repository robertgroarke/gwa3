"""Async client for Kamadan trade chat price history APIs.

Queries two independent Guild Wars trade chat archives:
  1. kamadan.decltype.org  -- WebSocket search API
  2. kamadan.gwtoolbox.com -- HTTP JSON search API

Both return timestamped trade chat messages that the LLM can use
for price discovery before engaging in player-to-player trades.
"""

from __future__ import annotations

import asyncio
import json
import re
import time
from dataclasses import dataclass, field
from typing import Any

import httpx


# ---------------------------------------------------------------------------
# Data types
# ---------------------------------------------------------------------------

@dataclass
class TradeMessage:
    """A single trade chat message from a Kamadan archive."""
    seller: str
    message: str
    timestamp: int  # unix epoch seconds
    source: str     # "decltype" or "gwtoolbox"

    def to_dict(self) -> dict[str, Any]:
        return {
            "seller": self.seller,
            "message": self.message,
            "timestamp": self.timestamp,
            "source": self.source,
        }


@dataclass
class SearchResult:
    """Aggregated search result from one or both APIs."""
    query: str
    results: list[TradeMessage] = field(default_factory=list)
    total_results: int = 0
    errors: list[str] = field(default_factory=list)

    def to_dict(self) -> dict[str, Any]:
        return {
            "query": self.query,
            "results": [r.to_dict() for r in self.results],
            "total_results": self.total_results,
            "errors": self.errors,
        }


# ---------------------------------------------------------------------------
# decltype.org client  (WebSocket search)
# ---------------------------------------------------------------------------

_DECLTYPE_WS_URL = "wss://kamadan.decltype.org/ws"
_DECLTYPE_HTTP_URL = "https://kamadan.decltype.org"


async def _search_decltype_ws(
    query: str,
    count: int = 25,
    *,
    timeout: float = 10.0,
) -> tuple[list[TradeMessage], int, str | None]:
    """Search kamadan.decltype.org via its WebSocket API.

    Returns (messages, total_count, error_or_None).
    """
    try:
        import websockets  # type: ignore[import-untyped]
    except ImportError:
        # Fall back to HTTP scraping if websockets not installed
        return await _search_decltype_http(query, count, timeout=timeout)

    messages: list[TradeMessage] = []
    total = 0
    try:
        async with asyncio.timeout(timeout):
            async with websockets.connect(_DECLTYPE_WS_URL) as ws:
                await ws.send(json.dumps({
                    "query": query,
                    "offset": 0,
                    "suggest": False,
                }))
                raw = await ws.recv()
                data = json.loads(raw)
                total = data.get("num_results", 0)
                for r in data.get("results", [])[:count]:
                    messages.append(TradeMessage(
                        seller=r.get("name", ""),
                        message=r.get("message", ""),
                        timestamp=r.get("timestamp", 0),
                        source="decltype",
                    ))
        return messages, total, None
    except Exception as e:
        # WebSocket failed, try HTTP fallback
        return await _search_decltype_http(query, count, timeout=timeout)


async def _search_decltype_http(
    query: str,
    count: int = 25,
    *,
    timeout: float = 10.0,
) -> tuple[list[TradeMessage], int, str | None]:
    """Fallback: scrape kamadan.decltype.org search results via HTTP."""
    import re as _re

    url = f"{_DECLTYPE_HTTP_URL}/search/{httpx.URL(query).raw_path.decode() if False else query}/0"
    messages: list[TradeMessage] = []
    total = 0
    try:
        async with httpx.AsyncClient(timeout=timeout, follow_redirects=True) as client:
            resp = await client.get(
                f"{_DECLTYPE_HTTP_URL}/search/{query}/0",
                headers={"User-Agent": "gwa3-bridge/1.0"},
            )
            resp.raise_for_status()
            html = resp.text

            # Parse total from "1-25 out of about N results"
            m = _re.search(r"out of about\s+([\d,]+)\s+results", html)
            if m:
                total = int(m.group(1).replace(",", ""))

            # Parse message rows -- each result is in a table row or list item
            # The HTML uses <td> elements: name, timestamp, message
            rows = _re.findall(
                r'<td[^>]*class="[^"]*name[^"]*"[^>]*>(.*?)</td>'
                r'.*?<td[^>]*class="[^"]*message[^"]*"[^>]*>(.*?)</td>',
                html,
                _re.DOTALL,
            )
            for name_html, msg_html in rows[:count]:
                name = _re.sub(r"<[^>]+>", "", name_html).strip()
                message = _re.sub(r"<[^>]+>", "", msg_html).strip()
                if name and message:
                    messages.append(TradeMessage(
                        seller=name,
                        message=message,
                        timestamp=0,
                        source="decltype",
                    ))

        return messages, total, None
    except Exception as e:
        return [], 0, f"decltype HTTP error: {e}"


# ---------------------------------------------------------------------------
# gwtoolbox.com client  (HTTP JSON)
# ---------------------------------------------------------------------------

_GWTOOLBOX_URL = "https://kamadan.gwtoolbox.com"


async def _search_gwtoolbox(
    query: str,
    count: int = 25,
    *,
    timeout: float = 10.0,
) -> tuple[list[TradeMessage], int, str | None]:
    """Search kamadan.gwtoolbox.com via HTTP.

    The GWToolbox Kamadan archive exposes a JSON search endpoint.
    """
    messages: list[TradeMessage] = []
    total = 0
    try:
        async with httpx.AsyncClient(timeout=timeout, follow_redirects=True) as client:
            resp = await client.get(
                f"{_GWTOOLBOX_URL}/search/{query}/0",
                headers={
                    "User-Agent": "gwa3-bridge/1.0",
                    "Accept": "application/json, text/html",
                },
            )
            resp.raise_for_status()

            content_type = resp.headers.get("content-type", "")
            if "json" in content_type:
                data = resp.json()
                total = data.get("num_results", data.get("total", 0))
                for r in data.get("results", [])[:count]:
                    messages.append(TradeMessage(
                        seller=r.get("name", r.get("seller", "")),
                        message=r.get("message", ""),
                        timestamp=r.get("timestamp", 0),
                        source="gwtoolbox",
                    ))
            else:
                # HTML response -- parse similarly to decltype
                html = resp.text
                m = re.search(r"(\d[\d,]*)\s+results?", html)
                if m:
                    total = int(m.group(1).replace(",", ""))

                rows = re.findall(
                    r'<td[^>]*class="[^"]*name[^"]*"[^>]*>(.*?)</td>'
                    r'.*?<td[^>]*class="[^"]*message[^"]*"[^>]*>(.*?)</td>',
                    html,
                    re.DOTALL,
                )
                for name_html, msg_html in rows[:count]:
                    name = re.sub(r"<[^>]+>", "", name_html).strip()
                    message = re.sub(r"<[^>]+>", "", msg_html).strip()
                    if name and message:
                        messages.append(TradeMessage(
                            seller=name,
                            message=message,
                            timestamp=0,
                            source="gwtoolbox",
                        ))

        return messages, total, None
    except Exception as e:
        return [], 0, f"gwtoolbox error: {e}"


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

class KamadanClient:
    """Async client that queries both Kamadan trade archives in parallel.

    Usage::

        client = KamadanClient()
        result = await client.search("Ecto", count=10)
        for msg in result.results:
            print(f"{msg.seller}: {msg.message}")
    """

    def __init__(
        self,
        *,
        use_decltype: bool = True,
        use_gwtoolbox: bool = True,
        timeout: float = 10.0,
        cache_ttl: float = 120.0,
    ):
        self.use_decltype = use_decltype
        self.use_gwtoolbox = use_gwtoolbox
        self.timeout = timeout
        self.cache_ttl = cache_ttl
        self._cache: dict[str, tuple[float, SearchResult]] = {}

    def _cache_key(self, query: str, count: int) -> str:
        return f"{query.lower().strip()}:{count}"

    def _get_cached(self, key: str) -> SearchResult | None:
        if key in self._cache:
            ts, result = self._cache[key]
            if time.monotonic() - ts < self.cache_ttl:
                return result
            del self._cache[key]
        return None

    def _put_cache(self, key: str, result: SearchResult) -> None:
        # Evict old entries if cache grows large
        if len(self._cache) > 200:
            cutoff = time.monotonic() - self.cache_ttl
            self._cache = {
                k: (t, r) for k, (t, r) in self._cache.items() if t > cutoff
            }
        self._cache[key] = (time.monotonic(), result)

    async def search(self, query: str, count: int = 25) -> SearchResult:
        """Search both Kamadan archives for trade messages matching *query*.

        Results from both sources are merged, deduplicated by message content,
        and sorted by timestamp (newest first). Cached for *cache_ttl* seconds.
        """
        query = query.strip()
        if not query:
            return SearchResult(query=query, errors=["empty query"])

        key = self._cache_key(query, count)
        cached = self._get_cached(key)
        if cached is not None:
            return cached

        tasks: list[asyncio.Task] = []
        if self.use_decltype:
            tasks.append(asyncio.create_task(
                _search_decltype_ws(query, count, timeout=self.timeout)
            ))
        if self.use_gwtoolbox:
            tasks.append(asyncio.create_task(
                _search_gwtoolbox(query, count, timeout=self.timeout)
            ))

        all_messages: list[TradeMessage] = []
        total = 0
        errors: list[str] = []

        for coro in asyncio.as_completed(tasks):
            msgs, cnt, err = await coro
            all_messages.extend(msgs)
            total = max(total, cnt)
            if err:
                errors.append(err)

        # Deduplicate by (seller, message)
        seen: set[tuple[str, str]] = set()
        unique: list[TradeMessage] = []
        for msg in all_messages:
            key_pair = (msg.seller, msg.message)
            if key_pair not in seen:
                seen.add(key_pair)
                unique.append(msg)

        # Sort by timestamp descending (newest first)
        unique.sort(key=lambda m: m.timestamp, reverse=True)

        result = SearchResult(
            query=query,
            results=unique[:count],
            total_results=total,
            errors=errors,
        )

        cache_key = self._cache_key(query, count)
        self._put_cache(cache_key, result)

        return result

    async def search_for_llm(self, query: str, count: int = 10) -> dict[str, Any]:
        """Search and return a dict formatted for LLM tool-call results.

        Strips down the output to what the LLM needs for price reasoning:
        seller names, messages, and relative recency.
        """
        result = await self.search(query, count=count)

        now = int(time.time())
        condensed = []
        for msg in result.results:
            entry: dict[str, Any] = {
                "seller": msg.seller,
                "message": msg.message,
            }
            if msg.timestamp > 0:
                age_min = max(0, (now - msg.timestamp)) // 60
                if age_min < 60:
                    entry["age"] = f"{age_min}m ago"
                elif age_min < 1440:
                    entry["age"] = f"{age_min // 60}h ago"
                else:
                    entry["age"] = f"{age_min // 1440}d ago"
            condensed.append(entry)

        return {
            "query": result.query,
            "matches": condensed,
            "total_available": result.total_results,
            "errors": result.errors if result.errors else None,
        }

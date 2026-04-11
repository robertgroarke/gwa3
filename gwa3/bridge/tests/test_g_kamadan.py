"""Category G: Kamadan trade chat price history client -- unit tests (no game pipe needed).

These tests mock HTTP/WebSocket responses so they run without any game injection.
Run with:  python -m unittest bridge.tests.test_g_kamadan -v
   or:     python -m bridge.tests --filter "test_kamadan*"

## Test plan — strict pass/fail criteria
##
## T1:  WS import fallback    -- _search_decltype_ws falls back to HTTP when websockets missing
## T2:  WS connection failure  -- WS connects then errors, falls back to HTTP
## T3:  WS happy path          -- mock websockets, verify JSON parse + TradeMessage fields
## T4:  Cache expiration        -- stale entry not returned after TTL
## T5:  Cache case-insensitive  -- "Ecto" and "ecto" share one cache slot
## T6:  Cache eviction >200     -- old entries pruned, cache doesn't grow unbounded
## T7:  Count clamping          -- results list never exceeds requested count
## T8:  Both sources disabled   -- empty results, no crash
## T9:  Both sources fail       -- errors from both backends collected
## T10: Empty backend results   -- valid response, zero matches
## T11: Malformed HTML          -- no matching <td> structure -> 0 results, no crash
## T12: Special chars in query  -- spaces, ampersands don't crash URL construction
## T13: LLM error propagation   -- both backends error -> errors field is list
## T14: Timestamp edge cases    -- future timestamp shows "0m ago", not negative
## T15: Agent loop dispatch     -- search_trade_prices handled locally, not sent to pipe
## T16: Agent loop count clamp  -- count >25 clamped to 25
## T17: Agent loop error        -- kamadan failure -> JSON error, doesn't crash loop
## T18: GWToolbox seller key    -- accepts both 'name' and 'seller' JSON fields
"""

import asyncio
import json
import sys
import time
import types
import unittest
from unittest.mock import AsyncMock, MagicMock, patch

from ..kamadan_client import (
    KamadanClient,
    SearchResult,
    TradeMessage,
    _search_decltype_http,
    _search_decltype_ws,
    _search_gwtoolbox,
)


def _run(coro):
    """Run an async coroutine synchronously."""
    return asyncio.run(coro)


# ---------------------------------------------------------------------------
# Reusable mock HTTP fixtures
# ---------------------------------------------------------------------------

DECLTYPE_HTML = """
<html><body>
<p>1-25 out of about 5,432 results (took 0.5ms)</p>
<table>
<tr>
  <td class="name">Player One</td>
  <td class="timestamp">2 minutes ago</td>
  <td class="message">WTS Ecto 4e ea pm me</td>
</tr>
<tr>
  <td class="name">Trader Bob</td>
  <td class="timestamp">5 minutes ago</td>
  <td class="message">WTB Ectos 3.5e each, buying 100</td>
</tr>
<tr>
  <td class="name">Rich Guy</td>
  <td class="timestamp">10 minutes ago</td>
  <td class="message">WTS 250 Ectos 4e ea bulk discount</td>
</tr>
</table>
</body></html>
"""

GWTOOLBOX_JSON = {
    "num_results": 12345,
    "results": [
        {"name": "Seller A", "message": "WTS Ecto 4e", "timestamp": int(time.time()) - 60},
        {"name": "Buyer B", "message": "WTB Ecto 3e", "timestamp": int(time.time()) - 300},
        {"name": "Seller C", "message": "WTS Ectos 5/20e", "timestamp": int(time.time()) - 600},
    ],
}

GWTOOLBOX_HTML = """
<html><body>
<p>Showing 3 results</p>
<table>
<tr>
  <td class="name">HTML Seller</td>
  <td class="message">WTS Ecto 4e each</td>
</tr>
</table>
</body></html>
"""


def _mock_http_client(response_mock):
    """Build a fully mocked httpx.AsyncClient context manager."""
    mock_client = AsyncMock()
    mock_client.get = AsyncMock(return_value=response_mock)
    mock_client.__aenter__ = AsyncMock(return_value=mock_client)
    mock_client.__aexit__ = AsyncMock(return_value=False)
    return mock_client


def _json_response(data, content_type="application/json"):
    resp = MagicMock()
    resp.status_code = 200
    resp.headers = {"content-type": content_type}
    resp.json = MagicMock(return_value=data)
    resp.raise_for_status = MagicMock()
    return resp


def _html_response(html):
    resp = MagicMock()
    resp.status_code = 200
    resp.headers = {"content-type": "text/html"}
    resp.text = html
    resp.raise_for_status = MagicMock()
    return resp


def _error_client(exc):
    mock_client = AsyncMock()
    mock_client.get = AsyncMock(side_effect=exc)
    mock_client.__aenter__ = AsyncMock(return_value=mock_client)
    mock_client.__aexit__ = AsyncMock(return_value=False)
    return mock_client


# ---------------------------------------------------------------------------
# Data model tests
# ---------------------------------------------------------------------------

class TestTradeMessage(unittest.TestCase):
    def test_to_dict_all_fields(self):
        """PASS: to_dict returns exactly {seller, message, timestamp, source}."""
        msg = TradeMessage("Test Player", "WTS Ecto 4e", 1700000000, "decltype")
        d = msg.to_dict()
        self.assertEqual(set(d.keys()), {"seller", "message", "timestamp", "source"})
        self.assertEqual(d["seller"], "Test Player")
        self.assertEqual(d["message"], "WTS Ecto 4e")
        self.assertEqual(d["timestamp"], 1700000000)
        self.assertEqual(d["source"], "decltype")

    def test_search_result_to_dict(self):
        """PASS: SearchResult.to_dict nests child TradeMessage dicts."""
        result = SearchResult(
            query="Ecto",
            results=[TradeMessage("A", "WTS Ecto", 100, "decltype")],
            total_results=42,
            errors=["some error"],
        )
        d = result.to_dict()
        self.assertEqual(d["query"], "Ecto")
        self.assertEqual(len(d["results"]), 1)
        self.assertIsInstance(d["results"][0], dict)
        self.assertEqual(d["total_results"], 42)
        self.assertEqual(d["errors"], ["some error"])

    def test_search_result_empty(self):
        """PASS: empty SearchResult serializes cleanly."""
        result = SearchResult(query="nothing")
        d = result.to_dict()
        self.assertEqual(d["results"], [])
        self.assertEqual(d["total_results"], 0)
        self.assertEqual(d["errors"], [])


# ---------------------------------------------------------------------------
# T1-T3: WebSocket path tests
# ---------------------------------------------------------------------------

class TestDectypeWebSocket(unittest.TestCase):
    def test_t1_ws_import_fallback(self):
        """T1 PASS: when websockets is not installed, _search_decltype_ws
        falls back to _search_decltype_http and returns results (not ImportError)."""
        # Temporarily hide the websockets module
        saved = sys.modules.get("websockets")
        sys.modules["websockets"] = None  # force ImportError on import

        try:
            with patch("bridge.kamadan_client.httpx.AsyncClient") as mock_cls:
                mock_cls.return_value = _mock_http_client(_html_response(DECLTYPE_HTML))
                msgs, total, err = _run(_search_decltype_ws("Ecto", count=5))

                # Must not crash; must return results from HTTP fallback
                self.assertIsNone(err)
                self.assertGreaterEqual(len(msgs), 1)
                self.assertEqual(msgs[0].source, "decltype")
        finally:
            if saved is not None:
                sys.modules["websockets"] = saved
            else:
                sys.modules.pop("websockets", None)

    def test_t2_ws_connection_failure_falls_back(self):
        """T2 PASS: when WebSocket connect raises, falls back to HTTP and
        returns results (not an exception)."""
        # Create a fake websockets module whose connect() raises
        fake_ws = types.ModuleType("websockets")
        fake_ws.connect = MagicMock(side_effect=Exception("WS refused"))
        saved = sys.modules.get("websockets")
        sys.modules["websockets"] = fake_ws

        try:
            with patch("bridge.kamadan_client.httpx.AsyncClient") as mock_cls:
                mock_cls.return_value = _mock_http_client(_html_response(DECLTYPE_HTML))
                msgs, total, err = _run(_search_decltype_ws("Ecto", count=5))

                self.assertIsNone(err)
                self.assertGreaterEqual(len(msgs), 1)
        finally:
            if saved is not None:
                sys.modules["websockets"] = saved
            else:
                sys.modules.pop("websockets", None)

    def test_t3_ws_happy_path(self):
        """T3 PASS: with a mock websockets module, _search_decltype_ws parses
        the JSON response and returns correct TradeMessage fields."""
        now = int(time.time())
        ws_response = json.dumps({
            "num_results": 999,
            "results": [
                {"name": "WS Seller", "message": "WTS Ecto 5e", "timestamp": now - 30},
                {"name": "WS Buyer", "message": "WTB Ecto 4e", "timestamp": now - 120},
            ],
        })

        # Build a fake websockets module with async context manager
        mock_ws_conn = AsyncMock()
        mock_ws_conn.send = AsyncMock()
        mock_ws_conn.recv = AsyncMock(return_value=ws_response)
        mock_ws_conn.__aenter__ = AsyncMock(return_value=mock_ws_conn)
        mock_ws_conn.__aexit__ = AsyncMock(return_value=False)

        fake_ws = types.ModuleType("websockets")
        fake_ws.connect = MagicMock(return_value=mock_ws_conn)

        saved = sys.modules.get("websockets")
        sys.modules["websockets"] = fake_ws

        try:
            msgs, total, err = _run(_search_decltype_ws("Ecto", count=25))

            self.assertIsNone(err)
            self.assertEqual(total, 999)
            self.assertEqual(len(msgs), 2)
            self.assertEqual(msgs[0].seller, "WS Seller")
            self.assertEqual(msgs[0].message, "WTS Ecto 5e")
            self.assertEqual(msgs[0].timestamp, now - 30)
            self.assertEqual(msgs[0].source, "decltype")
            self.assertEqual(msgs[1].seller, "WS Buyer")
        finally:
            if saved is not None:
                sys.modules["websockets"] = saved
            else:
                sys.modules.pop("websockets", None)


# ---------------------------------------------------------------------------
# decltype HTTP fallback tests
# ---------------------------------------------------------------------------

class TestDectypeHttp(unittest.TestCase):
    @patch("bridge.kamadan_client.httpx.AsyncClient")
    def test_parses_html_results(self, mock_cls):
        """PASS: parses 3 results from HTML, total=5432, source=decltype."""
        mock_cls.return_value = _mock_http_client(_html_response(DECLTYPE_HTML))
        msgs, total, err = _run(_search_decltype_http("Ecto", count=25))

        self.assertIsNone(err)
        self.assertEqual(total, 5432)
        self.assertEqual(len(msgs), 3)
        self.assertEqual(msgs[0].source, "decltype")
        self.assertEqual(msgs[0].seller, "Player One")
        self.assertIn("WTS Ecto", msgs[0].message)

    @patch("bridge.kamadan_client.httpx.AsyncClient")
    def test_handles_http_error(self, mock_cls):
        """PASS: network error returns ([], 0, 'decltype HTTP error: ...')."""
        mock_cls.return_value = _error_client(Exception("connection refused"))
        msgs, total, err = _run(_search_decltype_http("Ecto"))

        self.assertEqual(msgs, [])
        self.assertEqual(total, 0)
        self.assertIsNotNone(err)
        self.assertIn("decltype HTTP error", err)

    @patch("bridge.kamadan_client.httpx.AsyncClient")
    def test_t11_malformed_html(self, mock_cls):
        """T11 PASS: HTML with no matching <td class='name'> returns 0 results, no crash."""
        bad_html = "<html><body><p>No table here at all</p></body></html>"
        mock_cls.return_value = _mock_http_client(_html_response(bad_html))
        msgs, total, err = _run(_search_decltype_http("Ecto"))

        self.assertIsNone(err)
        self.assertEqual(msgs, [])
        self.assertEqual(total, 0)


# ---------------------------------------------------------------------------
# gwtoolbox tests
# ---------------------------------------------------------------------------

class TestGwtoolbox(unittest.TestCase):
    @patch("bridge.kamadan_client.httpx.AsyncClient")
    def test_parses_json_response(self, mock_cls):
        """PASS: JSON response -> 3 results, total=12345, source=gwtoolbox."""
        mock_cls.return_value = _mock_http_client(_json_response(GWTOOLBOX_JSON))
        msgs, total, err = _run(_search_gwtoolbox("Ecto", count=25))

        self.assertIsNone(err)
        self.assertEqual(total, 12345)
        self.assertEqual(len(msgs), 3)
        self.assertEqual(msgs[0].seller, "Seller A")
        self.assertEqual(msgs[0].source, "gwtoolbox")

    @patch("bridge.kamadan_client.httpx.AsyncClient")
    def test_parses_html_fallback(self, mock_cls):
        """PASS: text/html content-type triggers HTML parser, extracts results."""
        mock_cls.return_value = _mock_http_client(_html_response(GWTOOLBOX_HTML))
        msgs, total, err = _run(_search_gwtoolbox("Ecto"))

        self.assertIsNone(err)
        self.assertEqual(total, 3)
        self.assertGreaterEqual(len(msgs), 1)
        self.assertEqual(msgs[0].seller, "HTML Seller")

    @patch("bridge.kamadan_client.httpx.AsyncClient")
    def test_handles_error(self, mock_cls):
        """PASS: exception -> ([], 0, 'gwtoolbox error: ...')."""
        mock_cls.return_value = _error_client(Exception("403 Forbidden"))
        msgs, total, err = _run(_search_gwtoolbox("Ecto"))

        self.assertEqual(msgs, [])
        self.assertIsNotNone(err)
        self.assertIn("gwtoolbox error", err)

    @patch("bridge.kamadan_client.httpx.AsyncClient")
    def test_t18_seller_key_variant(self, mock_cls):
        """T18 PASS: gwtoolbox JSON with 'seller' key instead of 'name'
        still extracts seller correctly."""
        data = {
            "num_results": 1,
            "results": [
                {"seller": "AltKey Player", "message": "WTS stuff", "timestamp": 1000},
            ],
        }
        mock_cls.return_value = _mock_http_client(_json_response(data))
        msgs, total, err = _run(_search_gwtoolbox("stuff"))

        self.assertIsNone(err)
        self.assertEqual(len(msgs), 1)
        self.assertEqual(msgs[0].seller, "AltKey Player")

    @patch("bridge.kamadan_client.httpx.AsyncClient")
    def test_t10_empty_json_results(self, mock_cls):
        """T10 PASS: valid JSON with empty results list returns 0 messages."""
        data = {"num_results": 0, "results": []}
        mock_cls.return_value = _mock_http_client(_json_response(data))
        msgs, total, err = _run(_search_gwtoolbox("xyznonexistent"))

        self.assertIsNone(err)
        self.assertEqual(msgs, [])
        self.assertEqual(total, 0)


# ---------------------------------------------------------------------------
# KamadanClient integration tests (mocked backends)
# ---------------------------------------------------------------------------

class TestKamadanClient(unittest.TestCase):
    def test_empty_query_returns_error(self):
        """PASS: empty string query -> errors=['empty query'], results=[]."""
        result = _run(KamadanClient().search(""))
        self.assertEqual(result.errors, ["empty query"])
        self.assertEqual(result.results, [])

    def test_empty_whitespace_query(self):
        """PASS: whitespace-only query -> errors=['empty query']."""
        result = _run(KamadanClient().search("   "))
        self.assertEqual(result.errors, ["empty query"])

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_merges_both_sources(self, mock_decltype, mock_gwtoolbox):
        """PASS: results from both backends merged, sorted newest-first,
        total_results = max(both)."""
        now = int(time.time())
        mock_decltype.return_value = (
            [TradeMessage("A", "WTS Ecto 4e", now - 60, "decltype")], 100, None,
        )
        mock_gwtoolbox.return_value = (
            [TradeMessage("B", "WTB Ecto 3e", now - 30, "gwtoolbox")], 200, None,
        )

        result = _run(KamadanClient().search("Ecto"))

        self.assertEqual(len(result.results), 2)
        self.assertEqual(result.total_results, 200)
        self.assertEqual(result.results[0].seller, "B")  # newer first
        self.assertEqual(result.results[1].seller, "A")

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_deduplicates_results(self, mock_decltype, mock_gwtoolbox):
        """PASS: same (seller, message) from both backends -> 1 result."""
        now = int(time.time())
        mock_decltype.return_value = (
            [TradeMessage("Same", "WTS Ecto 4e", now, "decltype")], 10, None,
        )
        mock_gwtoolbox.return_value = (
            [TradeMessage("Same", "WTS Ecto 4e", now, "gwtoolbox")], 10, None,
        )
        result = _run(KamadanClient().search("Ecto"))
        self.assertEqual(len(result.results), 1)

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_caches_results(self, mock_decltype, mock_gwtoolbox):
        """PASS: second identical search hits cache (backend called once)."""
        mock_decltype.return_value = (
            [TradeMessage("A", "WTS", 100, "decltype")], 1, None,
        )
        mock_gwtoolbox.return_value = ([], 0, None)

        client = KamadanClient(cache_ttl=60.0)
        r1 = _run(client.search("Ecto"))
        r2 = _run(client.search("Ecto"))

        self.assertEqual(mock_decltype.call_count, 1)
        self.assertEqual(len(r1.results), len(r2.results))

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_partial_failure_still_returns(self, mock_decltype, mock_gwtoolbox):
        """PASS: one backend succeeds, one errors -> results + errors both present."""
        mock_decltype.return_value = (
            [TradeMessage("A", "WTS Ecto", 100, "decltype")], 50, None,
        )
        mock_gwtoolbox.return_value = ([], 0, "gwtoolbox error: 403")

        result = _run(KamadanClient().search("Ecto"))

        self.assertEqual(len(result.results), 1)
        self.assertEqual(len(result.errors), 1)
        self.assertIn("gwtoolbox", result.errors[0])

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_single_source_mode(self, mock_decltype, mock_gwtoolbox):
        """PASS: use_gwtoolbox=False -> gwtoolbox never called."""
        mock_decltype.return_value = (
            [TradeMessage("A", "WTS", 100, "decltype")], 1, None,
        )
        result = _run(KamadanClient(use_gwtoolbox=False).search("Ecto"))

        mock_gwtoolbox.assert_not_called()
        self.assertEqual(len(result.results), 1)

    # --- T4: Cache expiration ---

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_t4_cache_expiration(self, mock_decltype, mock_gwtoolbox):
        """T4 PASS: after TTL expires, cache miss triggers fresh backend call."""
        mock_decltype.return_value = (
            [TradeMessage("A", "WTS", 100, "decltype")], 1, None,
        )
        mock_gwtoolbox.return_value = ([], 0, None)

        client = KamadanClient(cache_ttl=0.0)  # immediate expiry
        _run(client.search("Ecto"))
        _run(client.search("Ecto"))

        # With TTL=0, every call is a cache miss -> 2 backend calls
        self.assertEqual(mock_decltype.call_count, 2)

    # --- T5: Cache case-insensitivity ---

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_t5_cache_case_insensitive(self, mock_decltype, mock_gwtoolbox):
        """T5 PASS: 'Ecto' and 'ecto' produce the same cache key -> 1 backend call."""
        mock_decltype.return_value = (
            [TradeMessage("A", "WTS", 100, "decltype")], 1, None,
        )
        mock_gwtoolbox.return_value = ([], 0, None)

        client = KamadanClient(cache_ttl=60.0)
        _run(client.search("Ecto"))
        _run(client.search("ecto"))

        self.assertEqual(mock_decltype.call_count, 1)

    # --- T6: Cache eviction ---

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_t6_cache_eviction(self, mock_decltype, mock_gwtoolbox):
        """T6 PASS: after >200 entries, old expired entries are pruned.
        Cache dict stays bounded."""
        mock_decltype.return_value = ([], 0, None)
        mock_gwtoolbox.return_value = ([], 0, None)

        client = KamadanClient(cache_ttl=0.0)  # everything expires immediately

        # Insert 205 entries (all immediately stale due to TTL=0)
        for i in range(205):
            _run(client.search(f"item_{i}"))

        # Now insert one more to trigger eviction
        _run(client.search("trigger_eviction"))

        # Eviction should have cleaned stale entries
        # After eviction + the new insert, cache should be much smaller than 206
        self.assertLessEqual(len(client._cache), 10)

    # --- T7: Count clamping ---

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_t7_count_clamping(self, mock_decltype, mock_gwtoolbox):
        """T7 PASS: requesting count=3 from backends returning 10 -> only 3 in results."""
        now = int(time.time())
        ten_msgs = [
            TradeMessage(f"P{i}", f"WTS item {i}", now - i * 10, "decltype")
            for i in range(10)
        ]
        mock_decltype.return_value = (ten_msgs, 100, None)
        mock_gwtoolbox.return_value = ([], 0, None)

        result = _run(KamadanClient().search("item", count=3))
        self.assertEqual(len(result.results), 3)

    # --- T8: Both sources disabled ---

    def test_t8_both_sources_disabled(self):
        """T8 PASS: use_decltype=False, use_gwtoolbox=False -> empty results, no crash."""
        client = KamadanClient(use_decltype=False, use_gwtoolbox=False)
        result = _run(client.search("Ecto"))

        self.assertEqual(result.results, [])
        self.assertEqual(result.total_results, 0)
        self.assertEqual(result.errors, [])

    # --- T9: Both sources fail ---

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_t9_both_sources_fail(self, mock_decltype, mock_gwtoolbox):
        """T9 PASS: both backends return errors -> errors list has 2 entries, results=[]."""
        mock_decltype.return_value = ([], 0, "decltype error: timeout")
        mock_gwtoolbox.return_value = ([], 0, "gwtoolbox error: 403")

        result = _run(KamadanClient().search("Ecto"))

        self.assertEqual(result.results, [])
        self.assertEqual(len(result.errors), 2)
        error_text = " ".join(result.errors)
        self.assertIn("decltype", error_text)
        self.assertIn("gwtoolbox", error_text)

    # --- T12: Special characters in query ---

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_t12_special_chars_in_query(self, mock_decltype, mock_gwtoolbox):
        """T12 PASS: query with spaces, ampersands, quotes doesn't crash."""
        mock_decltype.return_value = ([], 0, None)
        mock_gwtoolbox.return_value = ([], 0, None)

        for query in ["Ecto & Shard", "Glob of Ecto", 'WTS "item"', "a/b/c"]:
            result = _run(KamadanClient().search(query))
            self.assertIsInstance(result, SearchResult, f"crashed on query: {query!r}")


# ---------------------------------------------------------------------------
# search_for_llm output format tests
# ---------------------------------------------------------------------------

class TestSearchForLlm(unittest.TestCase):
    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_output_format(self, mock_decltype, mock_gwtoolbox):
        """PASS: output dict has {query, matches, total_available, errors}.
        Each match has {seller, message, age}."""
        now = int(time.time())
        mock_decltype.return_value = (
            [
                TradeMessage("Seller", "WTS Ecto 4e", now - 120, "decltype"),
                TradeMessage("Buyer", "WTB Ecto 3e", now - 7200, "decltype"),
            ],
            500, None,
        )
        mock_gwtoolbox.return_value = ([], 0, None)

        data = _run(KamadanClient().search_for_llm("Ecto", count=10))

        self.assertEqual(data["query"], "Ecto")
        self.assertIsInstance(data["matches"], list)
        self.assertEqual(len(data["matches"]), 2)
        self.assertEqual(data["total_available"], 500)
        self.assertIsNone(data["errors"])

        first = data["matches"][0]
        self.assertIn("seller", first)
        self.assertIn("message", first)
        self.assertIn("age", first)
        self.assertIn("m ago", first["age"])

        second = data["matches"][1]
        self.assertIn("h ago", second["age"])

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_age_days(self, mock_decltype, mock_gwtoolbox):
        """PASS: 2-day-old message shows 'd ago'."""
        now = int(time.time())
        mock_decltype.return_value = (
            [TradeMessage("Old", "WTS Ecto", now - 172800, "decltype")], 1, None,
        )
        mock_gwtoolbox.return_value = ([], 0, None)

        data = _run(KamadanClient().search_for_llm("Ecto"))
        self.assertIn("d ago", data["matches"][0]["age"])

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_no_timestamp_omits_age(self, mock_decltype, mock_gwtoolbox):
        """PASS: timestamp=0 -> no 'age' key in match dict."""
        mock_decltype.return_value = (
            [TradeMessage("NoTime", "WTS Ecto", 0, "decltype")], 1, None,
        )
        mock_gwtoolbox.return_value = ([], 0, None)

        data = _run(KamadanClient().search_for_llm("Ecto"))
        self.assertNotIn("age", data["matches"][0])

    # --- T13: LLM error propagation ---

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_t13_llm_errors_propagated(self, mock_decltype, mock_gwtoolbox):
        """T13 PASS: both backends error -> errors is a list (not None), matches=[]."""
        mock_decltype.return_value = ([], 0, "decltype down")
        mock_gwtoolbox.return_value = ([], 0, "gwtoolbox down")

        data = _run(KamadanClient().search_for_llm("Ecto"))

        self.assertIsInstance(data["errors"], list)
        self.assertEqual(len(data["errors"]), 2)
        self.assertEqual(data["matches"], [])

    # --- T14: Timestamp edge cases ---

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_t14_future_timestamp(self, mock_decltype, mock_gwtoolbox):
        """T14 PASS: future timestamp shows '0m ago', never negative."""
        future = int(time.time()) + 3600  # 1 hour in the future
        mock_decltype.return_value = (
            [TradeMessage("Future", "WTS Ecto", future, "decltype")], 1, None,
        )
        mock_gwtoolbox.return_value = ([], 0, None)

        data = _run(KamadanClient().search_for_llm("Ecto"))
        age = data["matches"][0]["age"]
        self.assertEqual(age, "0m ago")
        self.assertNotIn("-", age)


# ---------------------------------------------------------------------------
# Tool schema tests
# ---------------------------------------------------------------------------

class TestToolSchema(unittest.TestCase):
    def test_search_trade_prices_in_all_tools(self):
        """PASS: search_trade_prices is registered in ALL_TOOLS."""
        from ..tool_schema import ALL_TOOLS
        names = [t["function"]["name"] for t in ALL_TOOLS]
        self.assertIn("search_trade_prices", names)

    def test_search_trade_prices_schema(self):
        """PASS: schema has name, query (required string), count (optional int)."""
        from ..tool_schema import SEARCH_TRADE_PRICES
        func = SEARCH_TRADE_PRICES["function"]
        self.assertEqual(func["name"], "search_trade_prices")
        props = func["parameters"]["properties"]
        self.assertIn("query", props)
        self.assertEqual(props["query"]["type"], "string")
        self.assertIn("count", props)
        self.assertEqual(props["count"]["type"], "integer")
        self.assertEqual(func["parameters"]["required"], ["query"])


# ---------------------------------------------------------------------------
# T15-T17: Agent loop integration tests
# ---------------------------------------------------------------------------

class TestAgentLoopDispatch(unittest.TestCase):
    """Test that the agent loop handles search_trade_prices locally."""

    def _make_loop(self):
        """Build a minimal AgentLoop with mocked IPC and LLM."""
        from ..agent_loop import AgentLoop
        from ..llm_client import LLMResponse, ToolCall

        mock_ipc = MagicMock()
        mock_ipc.send_action = AsyncMock()
        mock_ipc.read_message = AsyncMock(return_value=None)

        mock_llm = MagicMock()

        loop = AgentLoop(ipc=mock_ipc, llm=mock_llm)
        return loop, mock_ipc, ToolCall

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_t15_dispatched_locally(self, mock_decltype, mock_gwtoolbox):
        """T15 PASS: search_trade_prices is handled locally; ipc.send_action
        is NOT called; result appears in history as tool role."""
        from ..llm_client import LLMResponse, ToolCall

        mock_decltype.return_value = (
            [TradeMessage("A", "WTS Ecto 4e", 1000, "decltype")], 50, None,
        )
        mock_gwtoolbox.return_value = ([], 0, None)

        loop, mock_ipc, _ = self._make_loop()

        response = LLMResponse(
            content=None,
            tool_calls=[
                ToolCall(id="tc_1", name="search_trade_prices", arguments='{"query": "Ecto"}'),
            ],
        )

        _run(loop._execute_tool_calls(response))

        # Must NOT have called ipc.send_action (this is a local tool)
        mock_ipc.send_action.assert_not_called()

        # Must have appended a tool result to history
        tool_msgs = [h for h in loop.history if h.get("role") == "tool"]
        self.assertEqual(len(tool_msgs), 1)
        content = json.loads(tool_msgs[0]["content"])
        self.assertIn("query", content)
        self.assertIn("matches", content)

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_t16_count_clamped_to_25(self, mock_decltype, mock_gwtoolbox):
        """T16 PASS: count=999 in tool call is clamped to 25 before calling search_for_llm."""
        mock_decltype.return_value = ([], 0, None)
        mock_gwtoolbox.return_value = ([], 0, None)

        loop, _, _ = self._make_loop()

        from ..llm_client import LLMResponse, ToolCall
        response = LLMResponse(
            content=None,
            tool_calls=[
                ToolCall(id="tc_2", name="search_trade_prices",
                         arguments='{"query": "Ecto", "count": 999}'),
            ],
        )

        # Spy on search_for_llm to check the count argument
        original = loop.kamadan.search_for_llm
        call_args = []

        async def spy(query, count=10):
            call_args.append(count)
            return await original(query, count=count)

        loop.kamadan.search_for_llm = spy
        _run(loop._execute_tool_calls(response))

        self.assertEqual(call_args, [25])

    def test_t17_error_resilience(self):
        """T17 PASS: when kamadan client raises, agent loop catches exception
        and writes JSON error to history (doesn't crash)."""
        from ..llm_client import LLMResponse, ToolCall

        loop, mock_ipc, _ = self._make_loop()

        # Make search_for_llm raise
        loop.kamadan.search_for_llm = AsyncMock(side_effect=RuntimeError("network down"))

        response = LLMResponse(
            content=None,
            tool_calls=[
                ToolCall(id="tc_3", name="search_trade_prices",
                         arguments='{"query": "Ecto"}'),
            ],
        )

        # Must not raise
        _run(loop._execute_tool_calls(response))

        tool_msgs = [h for h in loop.history if h.get("role") == "tool"]
        self.assertEqual(len(tool_msgs), 1)
        content = json.loads(tool_msgs[0]["content"])
        self.assertIn("error", content)
        self.assertIn("network down", content["error"])

        # ipc.send_action must NOT have been called
        mock_ipc.send_action.assert_not_called()


if __name__ == "__main__":
    unittest.main()

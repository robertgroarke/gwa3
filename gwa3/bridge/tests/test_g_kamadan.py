"""Category G: Kamadan trade chat price history client — unit tests (no game pipe needed).

These tests mock HTTP/WebSocket responses so they run without any game injection.
Run with:  python -m pytest gwa3/bridge/tests/test_g_kamadan.py
   or:     python -m bridge.tests --filter "test_kamadan*"
"""

import asyncio
import time
import unittest
from unittest.mock import AsyncMock, MagicMock, patch

from ..kamadan_client import (
    KamadanClient,
    SearchResult,
    TradeMessage,
    _search_decltype_http,
    _search_gwtoolbox,
)


def _run(coro):
    """Helper to run an async function synchronously."""
    return asyncio.run(coro)


# ---------------------------------------------------------------------------
# Mock HTTP responses
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


# ---------------------------------------------------------------------------
# TradeMessage / SearchResult tests
# ---------------------------------------------------------------------------

class TestTradeMessage(unittest.TestCase):
    def test_to_dict(self):
        msg = TradeMessage(
            seller="Test Player",
            message="WTS Ecto 4e",
            timestamp=1700000000,
            source="decltype",
        )
        d = msg.to_dict()
        self.assertEqual(d["seller"], "Test Player")
        self.assertEqual(d["message"], "WTS Ecto 4e")
        self.assertEqual(d["timestamp"], 1700000000)
        self.assertEqual(d["source"], "decltype")

    def test_search_result_to_dict(self):
        result = SearchResult(
            query="Ecto",
            results=[
                TradeMessage("A", "WTS Ecto", 100, "decltype"),
            ],
            total_results=42,
            errors=["some error"],
        )
        d = result.to_dict()
        self.assertEqual(d["query"], "Ecto")
        self.assertEqual(len(d["results"]), 1)
        self.assertEqual(d["total_results"], 42)
        self.assertEqual(d["errors"], ["some error"])


# ---------------------------------------------------------------------------
# decltype HTTP fallback tests
# ---------------------------------------------------------------------------

class TestDectypeHttp(unittest.TestCase):
    @patch("bridge.kamadan_client.httpx.AsyncClient")
    def test_parses_html_results(self, mock_client_cls):
        mock_resp = MagicMock()
        mock_resp.status_code = 200
        mock_resp.text = DECLTYPE_HTML
        mock_resp.raise_for_status = MagicMock()

        mock_client = AsyncMock()
        mock_client.get = AsyncMock(return_value=mock_resp)
        mock_client.__aenter__ = AsyncMock(return_value=mock_client)
        mock_client.__aexit__ = AsyncMock(return_value=False)
        mock_client_cls.return_value = mock_client

        msgs, total, err = _run(_search_decltype_http("Ecto", count=25))

        self.assertIsNone(err)
        self.assertEqual(total, 5432)
        self.assertGreaterEqual(len(msgs), 1)
        self.assertEqual(msgs[0].source, "decltype")

    @patch("bridge.kamadan_client.httpx.AsyncClient")
    def test_handles_http_error(self, mock_client_cls):
        mock_client = AsyncMock()
        mock_client.get = AsyncMock(side_effect=Exception("connection refused"))
        mock_client.__aenter__ = AsyncMock(return_value=mock_client)
        mock_client.__aexit__ = AsyncMock(return_value=False)
        mock_client_cls.return_value = mock_client

        msgs, total, err = _run(_search_decltype_http("Ecto"))

        self.assertEqual(msgs, [])
        self.assertEqual(total, 0)
        self.assertIn("decltype HTTP error", err)


# ---------------------------------------------------------------------------
# gwtoolbox tests
# ---------------------------------------------------------------------------

class TestGwtoolbox(unittest.TestCase):
    @patch("bridge.kamadan_client.httpx.AsyncClient")
    def test_parses_json_response(self, mock_client_cls):
        mock_resp = MagicMock()
        mock_resp.status_code = 200
        mock_resp.headers = {"content-type": "application/json"}
        mock_resp.json = MagicMock(return_value=GWTOOLBOX_JSON)
        mock_resp.raise_for_status = MagicMock()

        mock_client = AsyncMock()
        mock_client.get = AsyncMock(return_value=mock_resp)
        mock_client.__aenter__ = AsyncMock(return_value=mock_client)
        mock_client.__aexit__ = AsyncMock(return_value=False)
        mock_client_cls.return_value = mock_client

        msgs, total, err = _run(_search_gwtoolbox("Ecto", count=25))

        self.assertIsNone(err)
        self.assertEqual(total, 12345)
        self.assertEqual(len(msgs), 3)
        self.assertEqual(msgs[0].seller, "Seller A")
        self.assertEqual(msgs[0].source, "gwtoolbox")

    @patch("bridge.kamadan_client.httpx.AsyncClient")
    def test_parses_html_fallback(self, mock_client_cls):
        mock_resp = MagicMock()
        mock_resp.status_code = 200
        mock_resp.headers = {"content-type": "text/html"}
        mock_resp.text = GWTOOLBOX_HTML
        mock_resp.raise_for_status = MagicMock()

        mock_client = AsyncMock()
        mock_client.get = AsyncMock(return_value=mock_resp)
        mock_client.__aenter__ = AsyncMock(return_value=mock_client)
        mock_client.__aexit__ = AsyncMock(return_value=False)
        mock_client_cls.return_value = mock_client

        msgs, total, err = _run(_search_gwtoolbox("Ecto"))

        self.assertIsNone(err)
        self.assertEqual(total, 3)
        self.assertGreaterEqual(len(msgs), 1)

    @patch("bridge.kamadan_client.httpx.AsyncClient")
    def test_handles_error(self, mock_client_cls):
        mock_client = AsyncMock()
        mock_client.get = AsyncMock(side_effect=Exception("403 Forbidden"))
        mock_client.__aenter__ = AsyncMock(return_value=mock_client)
        mock_client.__aexit__ = AsyncMock(return_value=False)
        mock_client_cls.return_value = mock_client

        msgs, total, err = _run(_search_gwtoolbox("Ecto"))

        self.assertEqual(msgs, [])
        self.assertIn("gwtoolbox error", err)


# ---------------------------------------------------------------------------
# KamadanClient integration tests (mocked backends)
# ---------------------------------------------------------------------------

class TestKamadanClient(unittest.TestCase):
    def test_empty_query_returns_error(self):
        client = KamadanClient()
        result = _run(client.search(""))
        self.assertEqual(result.errors, ["empty query"])
        self.assertEqual(result.results, [])

    def test_empty_whitespace_query(self):
        client = KamadanClient()
        result = _run(client.search("   "))
        self.assertEqual(result.errors, ["empty query"])

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_merges_both_sources(self, mock_decltype, mock_gwtoolbox):
        now = int(time.time())
        mock_decltype.return_value = (
            [TradeMessage("A", "WTS Ecto 4e", now - 60, "decltype")],
            100,
            None,
        )
        mock_gwtoolbox.return_value = (
            [TradeMessage("B", "WTB Ecto 3e", now - 30, "gwtoolbox")],
            200,
            None,
        )

        client = KamadanClient()
        result = _run(client.search("Ecto"))

        self.assertEqual(len(result.results), 2)
        self.assertEqual(result.total_results, 200)  # max of both
        # Sorted by timestamp desc — gwtoolbox result is newer
        self.assertEqual(result.results[0].seller, "B")
        self.assertEqual(result.results[1].seller, "A")

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_deduplicates_results(self, mock_decltype, mock_gwtoolbox):
        now = int(time.time())
        same_msg = TradeMessage("Same Player", "WTS Ecto 4e", now, "decltype")
        mock_decltype.return_value = ([same_msg], 10, None)
        mock_gwtoolbox.return_value = (
            [TradeMessage("Same Player", "WTS Ecto 4e", now, "gwtoolbox")],
            10,
            None,
        )

        client = KamadanClient()
        result = _run(client.search("Ecto"))

        self.assertEqual(len(result.results), 1)

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_caches_results(self, mock_decltype, mock_gwtoolbox):
        mock_decltype.return_value = (
            [TradeMessage("A", "WTS", 100, "decltype")],
            1,
            None,
        )
        mock_gwtoolbox.return_value = ([], 0, None)

        client = KamadanClient(cache_ttl=60.0)
        r1 = _run(client.search("Ecto"))
        r2 = _run(client.search("Ecto"))

        # Second call should use cache — mock only called once
        self.assertEqual(mock_decltype.call_count, 1)
        self.assertEqual(r1.query, r2.query)
        self.assertEqual(len(r1.results), len(r2.results))

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_partial_failure_still_returns_results(self, mock_decltype, mock_gwtoolbox):
        mock_decltype.return_value = (
            [TradeMessage("A", "WTS Ecto", 100, "decltype")],
            50,
            None,
        )
        mock_gwtoolbox.return_value = ([], 0, "gwtoolbox error: 403")

        client = KamadanClient()
        result = _run(client.search("Ecto"))

        self.assertEqual(len(result.results), 1)
        self.assertEqual(len(result.errors), 1)
        self.assertIn("gwtoolbox", result.errors[0])

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_single_source_mode(self, mock_decltype, mock_gwtoolbox):
        mock_decltype.return_value = (
            [TradeMessage("A", "WTS", 100, "decltype")],
            1,
            None,
        )

        client = KamadanClient(use_gwtoolbox=False)
        result = _run(client.search("Ecto"))

        mock_gwtoolbox.assert_not_called()
        self.assertEqual(len(result.results), 1)


# ---------------------------------------------------------------------------
# search_for_llm output format tests
# ---------------------------------------------------------------------------

class TestSearchForLlm(unittest.TestCase):
    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_output_format(self, mock_decltype, mock_gwtoolbox):
        now = int(time.time())
        mock_decltype.return_value = (
            [
                TradeMessage("Seller", "WTS Ecto 4e", now - 120, "decltype"),
                TradeMessage("Buyer", "WTB Ecto 3e", now - 7200, "decltype"),
            ],
            500,
            None,
        )
        mock_gwtoolbox.return_value = ([], 0, None)

        client = KamadanClient()
        data = _run(client.search_for_llm("Ecto", count=10))

        self.assertEqual(data["query"], "Ecto")
        self.assertIsInstance(data["matches"], list)
        self.assertEqual(len(data["matches"]), 2)
        self.assertEqual(data["total_available"], 500)
        self.assertIsNone(data["errors"])

        # Check age formatting
        first = data["matches"][0]
        self.assertIn("seller", first)
        self.assertIn("message", first)
        self.assertIn("age", first)
        self.assertIn("m ago", first["age"])  # 2 minutes ago

        second = data["matches"][1]
        self.assertIn("h ago", second["age"])  # 2 hours ago

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_age_days(self, mock_decltype, mock_gwtoolbox):
        now = int(time.time())
        mock_decltype.return_value = (
            [TradeMessage("Old", "WTS Ecto", now - 172800, "decltype")],  # 2 days
            1,
            None,
        )
        mock_gwtoolbox.return_value = ([], 0, None)

        client = KamadanClient()
        data = _run(client.search_for_llm("Ecto"))

        self.assertIn("d ago", data["matches"][0]["age"])

    @patch("bridge.kamadan_client._search_gwtoolbox")
    @patch("bridge.kamadan_client._search_decltype_ws")
    def test_no_timestamp_omits_age(self, mock_decltype, mock_gwtoolbox):
        mock_decltype.return_value = (
            [TradeMessage("NoTime", "WTS Ecto", 0, "decltype")],
            1,
            None,
        )
        mock_gwtoolbox.return_value = ([], 0, None)

        client = KamadanClient()
        data = _run(client.search_for_llm("Ecto"))

        self.assertNotIn("age", data["matches"][0])


# ---------------------------------------------------------------------------
# Tool schema tests
# ---------------------------------------------------------------------------

class TestToolSchema(unittest.TestCase):
    def test_search_trade_prices_in_all_tools(self):
        from ..tool_schema import ALL_TOOLS
        names = [t["function"]["name"] for t in ALL_TOOLS]
        self.assertIn("search_trade_prices", names)

    def test_search_trade_prices_schema(self):
        from ..tool_schema import SEARCH_TRADE_PRICES
        func = SEARCH_TRADE_PRICES["function"]
        self.assertEqual(func["name"], "search_trade_prices")
        props = func["parameters"]["properties"]
        self.assertIn("query", props)
        self.assertEqual(props["query"]["type"], "string")
        self.assertEqual(func["parameters"]["required"], ["query"])


if __name__ == "__main__":
    unittest.main()

"""Kamadan source parser and backend unit tests."""

from .kamadan_test_support import *

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
    def test_parse_trade_html_helper(self):
        """PASS: shared HTML parser extracts rows, source, total, and honors count."""
        msgs, total = _parse_trade_html(DECLTYPE_HTML, source="decltype", count=2)

        self.assertEqual(total, 5432)
        self.assertEqual(len(msgs), 2)
        self.assertEqual(msgs[0].seller, "Player One")
        self.assertEqual(msgs[0].source, "decltype")
        self.assertEqual(msgs[1].message, "WTB Ectos 3.5e each, buying 100")

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

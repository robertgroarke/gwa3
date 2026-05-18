"""Kamadan client merge/cache and LLM formatting unit tests."""

from .kamadan_test_support import *

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

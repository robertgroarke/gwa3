"""Kamadan tool schema, agent dispatch, and live API tests."""

from .kamadan_test_support import *

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
        loop.kamadan.search_for_llm = AsyncMock(side_effect=ValueError("network down"))

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


@unittest.skipUnless(
    os.environ.get("GWA3_KAMADAN_LIVE_TESTS") == "1",
    "live API tests disabled",
)
class TestLiveAPI(unittest.TestCase):
    def test_live_decltype_search(self):
        """Live: decltype search returns at least one populated result for Ecto."""
        msgs, total, err = _run_with_timeout(
            _search_decltype_http("Ecto", count=5, timeout=15.0),
            timeout=15,
        )

        self.assertIsNone(err)
        self.assertGreater(total, 0)
        self.assertGreaterEqual(len(msgs), 1)
        for msg in msgs:
            self.assertTrue(msg.seller)
            self.assertTrue(msg.message)

    def test_live_gwtoolbox_search(self):
        """Live: gwtoolbox search returns at least one populated result for Ecto."""
        msgs, total, err = _run_with_timeout(
            _search_gwtoolbox("Ecto", count=5, timeout=15.0),
            timeout=15,
        )

        if err and ("403" in err or "timeout" in err.lower() or "timed out" in err.lower()):
            self.skipTest(f"gwtoolbox live test skipped: {err}")

        self.assertIsNone(err)
        self.assertGreater(total, 0)
        self.assertGreaterEqual(len(msgs), 1)
        for msg in msgs:
            self.assertTrue(msg.seller)
            self.assertTrue(msg.message)

    def test_live_full_client_search(self):
        """Live: full client search returns at least one result from either source."""
        client = KamadanClient(timeout=15.0, cache_ttl=0.0)
        result = _run_with_timeout(client.search("Ecto", count=5), timeout=15)

        self.assertGreaterEqual(len(result.results), 1)
        self.assertTrue(any(msg.source in {"decltype", "gwtoolbox"} for msg in result.results))
        self.assertGreater(result.total_results, 0)


if __name__ == "__main__":
    unittest.main()

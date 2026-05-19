import json
import unittest

from bridge.agent_loop import AgentLoop
from bridge.llm_client import LLMResponse, ToolCall
from bridge.trade_guard import TradeGuard


def _snapshot(*, player_items=None, partner_items=None, player_gold=0, partner_gold=0):
    return {
        "trade": {
            "is_open": True,
            "player": {
                "gold": player_gold,
                "items": player_items or [],
            },
            "partner": {
                "gold": partner_gold,
                "items": partner_items or [],
            },
        }
    }


class TradeGuardTests(unittest.TestCase):
    def test_accept_refuses_partner_offer_changed_after_submit(self):
        guard = TradeGuard()
        guard.record_submit_offer(_snapshot(partner_gold=100))

        result = guard.evaluate_accept_trade(_snapshot(partner_gold=50))

        self.assertIsNotNone(result)
        self.assertEqual(result["error"], "trade_unsafe:partner_offer_changed")

    def test_accept_refuses_value_imbalance(self):
        guard = TradeGuard(value_tolerance=1.1, kamadan_medians={100: 1000, 200: 100})
        same_partner_item = [{"item_id": 20, "model_id": 200, "quantity": 1, "value": 1}]
        player_item = [{"item_id": 10, "model_id": 100, "quantity": 1, "value": 1}]
        guard.record_submit_offer(_snapshot(partner_items=same_partner_item))

        result = guard.evaluate_accept_trade(_snapshot(
            player_items=player_item,
            partner_items=same_partner_item,
        ))

        self.assertIsNotNone(result)
        self.assertEqual(result["error"], "trade_unsafe:value_imbalance")

    def test_accept_refuses_hard_refusal_item(self):
        guard = TradeGuard()
        guard.record_submit_offer(_snapshot(partner_gold=1000))

        result = guard.evaluate_accept_trade(_snapshot(
            player_items=[{"item_id": 1, "full_name": "Account-bound Trophy", "value": 1}],
            partner_gold=1000,
        ))

        self.assertIsNotNone(result)
        self.assertEqual(result["error"], "trade_unsafe:hard_refusal_item")

    def test_whisper_refuses_common_scam_language(self):
        guard = TradeGuard()

        result = guard.evaluate_whisper("trust me, send first")

        self.assertIsNotNone(result)
        self.assertEqual(result["error"], "trade_unsafe:whisper_refused")


class AgentLoopTradeGuardTests(unittest.IsolatedAsyncioTestCase):
    async def test_accept_trade_guard_blocks_pipe_action(self):
        class DummyIpc:
            def __init__(self):
                self.sent = []

            async def send_action(self, name, params, req_id):
                self.sent.append((name, params, req_id))

        loop = AgentLoop(ipc=DummyIpc(), llm=object(), objective="trade safely")
        loop.observations.add_snapshot(_snapshot(player_gold=100, partner_gold=0))

        response = LLMResponse(tool_calls=[
            ToolCall(id="call-1", name="accept_trade", arguments="{}"),
        ])
        await loop._execute_tool_calls(response)

        self.assertEqual(loop.ipc.sent, [])
        self.assertEqual(len(loop.history), 1)
        result = json.loads(loop.history[0]["content"])
        self.assertEqual(result["error"], "trade_unsafe:no_submitted_offer_baseline")

    async def test_whisper_guard_blocks_pipe_action(self):
        class DummyIpc:
            def __init__(self):
                self.sent = []

            async def send_action(self, name, params, req_id):
                self.sent.append((name, params, req_id))

        loop = AgentLoop(ipc=DummyIpc(), llm=object(), objective="trade safely")

        response = LLMResponse(tool_calls=[
            ToolCall(
                id="call-1",
                name="send_whisper",
                arguments=json.dumps({"recipient": "Sample Trader", "message": "you first"}),
            ),
        ])
        await loop._execute_tool_calls(response)

        self.assertEqual(loop.ipc.sent, [])
        result = json.loads(loop.history[0]["content"])
        self.assertEqual(result["error"], "trade_unsafe:whisper_refused")


if __name__ == "__main__":
    unittest.main()

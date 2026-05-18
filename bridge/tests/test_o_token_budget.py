import asyncio
import sys
import unittest
from unittest.mock import patch

from bridge.agent_loop import AgentLoop
from bridge.config import parse_args
from bridge.llm_client import LLMResponse
from bridge.observation import ObservationWindow
from bridge.token_budget import (
    RemoteLlmNotAllowed,
    TokenBudgetExceeded,
    TokenBudgetGuard,
    extract_total_tokens,
)


class TokenBudgetTests(unittest.TestCase):
    def test_remote_openai_endpoint_requires_explicit_opt_in(self):
        with self.assertRaises(RemoteLlmNotAllowed):
            TokenBudgetGuard.for_openai_endpoint("https://api.example.com/v1")

        guard = TokenBudgetGuard.for_openai_endpoint(
            "https://api.example.com/v1",
            allow_remote=True,
        )

        self.assertTrue(guard.remote_backend)

    def test_local_openai_endpoint_is_allowed_by_default(self):
        guard = TokenBudgetGuard.for_openai_endpoint("http://127.0.0.1:8000/v1")

        self.assertFalse(guard.remote_backend)

    def test_codex_exec_provider_requires_explicit_opt_in(self):
        with self.assertRaises(RemoteLlmNotAllowed):
            TokenBudgetGuard.for_codex_exec()

        guard = TokenBudgetGuard.for_codex_exec(allow_remote=True)

        self.assertTrue(guard.remote_backend)

    def test_usage_accumulates_and_hard_stops_on_cap(self):
        guard = TokenBudgetGuard(hourly_token_cap=10)
        guard.record_usage({"prompt_tokens": 4, "completion_tokens": 5})

        with self.assertRaises(TokenBudgetExceeded):
            guard.record_usage({"total_tokens": 2})

    def test_extract_total_tokens_accepts_common_provider_shapes(self):
        self.assertEqual(extract_total_tokens({"total_tokens": 3}), 3)
        self.assertEqual(
            extract_total_tokens({"prompt_tokens": 2, "completion_tokens": 4}),
            6,
        )
        self.assertEqual(
            extract_total_tokens({"input_tokens": 5, "output_tokens": 7}),
            12,
        )

    def test_cli_exposes_budget_and_remote_opt_in(self):
        argv = [
            "bridge",
            "--allow-remote-llm",
            "--llm-hourly-token-cap",
            "12345",
        ]
        with patch.object(sys, "argv", argv):
            args = parse_args()

        self.assertTrue(args.allow_remote_llm)
        self.assertEqual(args.llm_hourly_token_cap, 12345)

    def test_legacy_advisory_flag_maps_to_advisory_autonomy(self):
        with patch.object(sys, "argv", ["bridge", "--advisory"]):
            args = parse_args()

        self.assertEqual(args.autonomy, "advisory")

    def test_tier1_budget_mode_removes_rich_snapshot_context(self):
        observations = ObservationWindow()
        observations.add_snapshot({
            "type": "snapshot",
            "tier": 3,
            "me": {"agent_id": 1, "hp": 1.0, "energy": 1.0},
            "map": {"map_id": 638, "loading_state": 1, "instance_time": 100},
            "inventory": {
                "gold_character": 1,
                "gold_storage": 2,
                "bags": [{"item_count": 3}],
                "free_slots_total": 4,
            },
            "agents": [{"id": 10, "allegiance": 3, "is_alive": True}],
        })

        full = observations.build_context_summary()
        tier1 = observations.build_context_summary(tier1_only=True)

        self.assertIn("Backpack:", full)
        self.assertIn("Nearby:", full)
        self.assertNotIn("Backpack:", tier1)
        self.assertNotIn("Nearby:", tier1)
        self.assertIn("Map:", tier1)


class AgentLoopTokenBudgetTests(unittest.IsolatedAsyncioTestCase):
    async def test_agent_loop_stops_when_budget_is_exceeded(self):
        class DummyIpc:
            async def read_message(self):
                return None

        class OverBudgetLlm:
            async def chat_completion(self, **kwargs):
                return LLMResponse(usage={"total_tokens": 11})

        guard = TokenBudgetGuard(hourly_token_cap=10)
        loop = AgentLoop(
            ipc=DummyIpc(),
            llm=OverBudgetLlm(),
            objective="Farm",
            token_budget=guard,
        )
        loop.observations.add_snapshot({
            "type": "snapshot",
            "tier": 1,
            "me": {"agent_id": 1, "hp": 1.0, "energy": 1.0},
            "map": {"map_id": 638, "loading_state": 1, "instance_time": 100},
        })

        await asyncio.wait_for(loop.run(), timeout=1.0)

        self.assertFalse(loop._running)

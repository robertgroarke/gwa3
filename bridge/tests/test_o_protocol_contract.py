import asyncio
import unittest

from bridge.agent_loop import AgentLoop
from bridge.ipc_client import IpcClient
from bridge.protocol import (
    IPC_PROTOCOL_VERSION,
    TOOL_SCHEMA_VERSION,
    ProtocolMismatchError,
    validate_protocol_version,
    with_protocol_version,
)


class ProtocolContractTests(unittest.TestCase):
    def test_protocol_version_is_added_without_mutating_source(self):
        source = {"type": "action", "name": "wait"}

        stamped = with_protocol_version(source)

        self.assertEqual(stamped["protocol_version"], IPC_PROTOCOL_VERSION)
        self.assertNotIn("protocol_version", source)

    def test_protocol_validator_rejects_mismatched_message(self):
        with self.assertRaises(ProtocolMismatchError):
            validate_protocol_version({"type": "snapshot", "protocol_version": 0}, "snapshot")

    def test_send_action_stamps_protocol_version(self):
        class RecordingClient(IpcClient):
            def __init__(self):
                super().__init__()
                self.sent = None

            async def send_message(self, msg: dict):
                self.sent = msg

        client = RecordingClient()

        asyncio.run(client.send_action("wait", {"milliseconds": 1}, "req-1"))

        self.assertEqual(client.sent["protocol_version"], IPC_PROTOCOL_VERSION)
        self.assertEqual(client.sent["type"], "action")
        self.assertEqual(client.sent["request_id"], "req-1")

    def test_agent_loop_emits_tool_schema_contract_in_llm_messages(self):
        class DummyIpc:
            async def read_message(self):
                return None

        class DummyLlm:
            pass

        loop = AgentLoop(ipc=DummyIpc(), llm=DummyLlm(), objective="Farm")

        messages = loop._build_messages()
        contract = "\n".join(
            msg["content"] for msg in messages if msg.get("role") == "system"
        )

        self.assertIn(f"ipc_protocol_version={IPC_PROTOCOL_VERSION}", contract)
        self.assertIn(f"tool_schema_version={TOOL_SCHEMA_VERSION}", contract)

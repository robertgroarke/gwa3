import unittest
from pathlib import Path

from bridge.ipc_client import IpcClient
from bridge.protocol import (
    IPC_PROTOCOL_VERSION,
    ProtocolMismatchError,
    validate_protocol_version,
    with_protocol_version,
)


ROOT = Path(__file__).resolve().parents[2]


class HandshakeContractTests(unittest.TestCase):
    def test_client_hello_includes_protocol_and_lane(self):
        client = IpcClient(r"\\.\pipe\gwa3_llm_sample")

        hello = with_protocol_version(client._client_hello_message())

        self.assertEqual(hello["type"], "hello")
        self.assertEqual(hello["role"], "bridge")
        self.assertEqual(hello["lane"], "sample")
        self.assertEqual(hello["protocol_version"], IPC_PROTOCOL_VERSION)
        self.assertEqual(hello["v"], IPC_PROTOCOL_VERSION)

    def test_short_version_field_is_accepted(self):
        validate_protocol_version({"type": "hello", "v": IPC_PROTOCOL_VERSION}, "client hello")

    def test_mismatched_version_is_rejected(self):
        with self.assertRaises(ProtocolMismatchError):
            validate_protocol_version({"type": "hello", "protocol_version": 0, "v": 0}, "client hello")

    def test_cpp_handshake_uses_shared_protocol_stamp_and_lane(self):
        source = (ROOT / "src/gwa3/llm/IpcServer.cpp").read_text(encoding="utf-8")

        self.assertIn("StampProtocol(hello)", source)
        self.assertIn('hello["lane"] = InferLaneName()', source)
        self.assertIn("ReadProtocolVersion(hello)", source)
        self.assertIn("Protocol mismatch", source)


if __name__ == "__main__":
    unittest.main()

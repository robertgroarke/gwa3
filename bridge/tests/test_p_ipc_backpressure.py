import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def read_repo_file(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


class IpcBackpressureContractTests(unittest.TestCase):
    def test_ipc_server_uses_overlapped_writes_with_timeout(self):
        source = read_repo_file("src/gwa3/llm/IpcServer.cpp")

        self.assertIn("FILE_FLAG_OVERLAPPED", source)
        self.assertIn("WRITE_TIMEOUT_MS = 2000", source)
        self.assertIn("PipeWriteAllWithTimeout", source)
        self.assertIn("WaitForOverlapped", source)
        self.assertIn("DisconnectClientAfterSendFailure", source)
        self.assertIn("DisconnectNamedPipe(g_pipe)", source)
        self.assertIn("ERROR_PIPE_NOT_CONNECTED", source)
        self.assertIn("connectedNow", source)

    def test_outbound_frames_are_queued_and_prioritized(self):
        header = read_repo_file("include/gwa3/llm/IpcServer.h")
        ipc_source = read_repo_file("src/gwa3/llm/IpcServer.cpp")
        bridge_source = read_repo_file("src/gwa3/llm/LlmBridge.cpp")
        event_source = read_repo_file("src/gwa3/llm/EventPush.cpp")
        action_source = read_repo_file("src/gwa3/llm/ActionExecutor.cpp")

        for priority in ("Snapshot", "Event", "Heartbeat", "ActionResult"):
            self.assertIn(priority, header)
        self.assertIn("g_outboundQueue", ipc_source)
        self.assertIn("SenderThread", ipc_source)
        self.assertIn("PopNextOutbound", ipc_source)
        self.assertIn("OutboundPriority::Snapshot", bridge_source)
        self.assertIn("OutboundPriority::Heartbeat", bridge_source)
        self.assertIn("OutboundPriority::Event", event_source)
        self.assertIn("OutboundPriority::ActionResult", action_source)

    def test_action_handlers_convert_bad_params_to_action_results(self):
        source = read_repo_file("src/gwa3/llm/ActionExecutor.cpp")

        self.assertIn("MakeBadParamsError", source)
        self.assertIn('MakeError("bad_params:json_parse")', source)
        self.assertIn("catch (const nlohmann::json::exception& e)", source)
        self.assertIn("SendResult(requestId, result.success, result.error)", source)
        self.assertNotIn("Fire-and-forget: skipping action_result", source)


if __name__ == "__main__":
    unittest.main()

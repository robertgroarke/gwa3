"""Tests for the Codex exec LLM provider adapter."""

import json
import unittest

from bridge.codex_exec_client import (
    CODEX_EXEC_RESPONSE_SCHEMA,
    CodexExecLLMClient,
    format_codex_exec_failure,
    parse_codex_exec_response,
)


FROGGY_TOOL = {
    "type": "function",
    "function": {
        "name": "froggy_run_dungeon_loop",
        "description": "Resume or run the Froggy dungeon loop from the current map.",
        "parameters": {
            "type": "object",
            "properties": {},
            "additionalProperties": False,
        },
    },
}


WAIT_TOOL = {
    "type": "function",
    "function": {
        "name": "wait",
        "description": "Wait briefly and observe again.",
        "parameters": {
            "type": "object",
            "properties": {
                "seconds": {"type": "number"},
            },
            "additionalProperties": False,
        },
    },
}


class CodexExecClientTests(unittest.TestCase):
    def test_parse_strict_adapter_shape(self):
        response = parse_codex_exec_response(
            json.dumps({
                "content": None,
                "tool_calls": [
                    {
                        "id": "call_1",
                        "name": "froggy_run_dungeon_loop",
                        "arguments": {},
                    }
                ],
            }),
            allowed_tools={"froggy_run_dungeon_loop"},
        )

        self.assertIsNone(response.content)
        self.assertEqual(response.finish_reason, "stop")
        self.assertEqual(len(response.tool_calls), 1)
        self.assertEqual(response.tool_calls[0].id, "call_1")
        self.assertEqual(response.tool_calls[0].name, "froggy_run_dungeon_loop")
        self.assertEqual(response.tool_calls[0].arguments, "{}")
        self.assertEqual(response.tool_calls[0].parsed_arguments, {})

    def test_parse_openai_style_tool_call_shape(self):
        response = parse_codex_exec_response(
            json.dumps({
                "content": "acting",
                "tool_calls": [
                    {
                        "id": "call_2",
                        "type": "function",
                        "function": {
                            "name": "wait",
                            "arguments": "{\"seconds\":1.5}",
                        },
                    }
                ],
            }),
            allowed_tools={"wait"},
        )

        self.assertEqual(response.content, "acting")
        self.assertEqual(len(response.tool_calls), 1)
        self.assertEqual(response.tool_calls[0].name, "wait")
        self.assertEqual(response.tool_calls[0].parsed_arguments, {"seconds": 1.5})

    def test_parse_markdown_fenced_json(self):
        response = parse_codex_exec_response(
            """```json
{"content":null,"tool_calls":[{"name":"wait","arguments":{"seconds":2}}]}
```""",
            allowed_tools={"wait"},
        )

        self.assertEqual(len(response.tool_calls), 1)
        self.assertEqual(response.tool_calls[0].id, "codex_call_1")
        self.assertEqual(response.tool_calls[0].parsed_arguments, {"seconds": 2})

    def test_parse_embedded_json_when_model_adds_text(self):
        response = parse_codex_exec_response(
            "Here is the decision:\n"
            "{\"content\":\"checking state\",\"tool_calls\":[]}\n"
            "No more text.",
            allowed_tools={"wait"},
        )

        self.assertEqual(response.content, "checking state")
        self.assertEqual(response.tool_calls, [])

    def test_unsupported_tool_is_skipped(self):
        response = parse_codex_exec_response(
            json.dumps({
                "content": None,
                "tool_calls": [
                    {"name": "delete_everything", "arguments": {}},
                    {"name": "wait", "arguments": {"seconds": 1}},
                ],
            }),
            allowed_tools={"wait"},
        )

        self.assertIn("Skipped unsupported tool", response.content)
        self.assertEqual(len(response.tool_calls), 1)
        self.assertEqual(response.tool_calls[0].name, "wait")

    def test_prompt_contains_compact_tools_and_messages(self):
        client = CodexExecLLMClient("gpt-5.3-codex-spark")
        prompt = client._build_prompt(
            [
                {"role": "system", "content": "system prompt"},
                {"role": "user", "content": "current state"},
            ],
            [FROGGY_TOOL, WAIT_TOOL],
        )

        self.assertIn("Return ONLY a JSON object", prompt)
        self.assertIn("froggy_run_dungeon_loop", prompt)
        self.assertIn("current state", prompt)

        encoded = prompt[prompt.index("{\"messages\""):]
        payload = json.loads(encoded)
        self.assertEqual(len(payload["messages"]), 2)
        self.assertEqual(len(payload["tools"]), 2)
        self.assertEqual(payload["tools"][0]["name"], "froggy_run_dungeon_loop")

    def test_failure_formatter_prioritizes_codex_error_lines(self):
        stderr = (
            "2026-05-16 WARN noisy plugin warning\n"
            "2026-05-16 WARN icon path must not contain '..'\n"
            "ERROR: You've hit your usage limit for GPT-5.3-Codex-Spark. "
            "Switch to another model now, or try again at 6:04 AM.\n"
        ).encode("utf-8")

        detail = format_codex_exec_failure(b"", stderr)

        self.assertIn("usage limit", detail)
        self.assertIn("6:04 AM", detail)
        self.assertNotIn("icon path", detail)

    def test_response_schema_constrains_tool_call_shape(self):
        self.assertEqual(CODEX_EXEC_RESPONSE_SCHEMA["type"], "object")
        self.assertFalse(CODEX_EXEC_RESPONSE_SCHEMA["additionalProperties"])
        self.assertEqual(
            CODEX_EXEC_RESPONSE_SCHEMA["required"],
            ["content", "tool_calls"],
        )
        tool_schema = (
            CODEX_EXEC_RESPONSE_SCHEMA["properties"]["tool_calls"]["items"]
        )
        self.assertIn("name", tool_schema["required"])
        self.assertIn("arguments", tool_schema["required"])
        self.assertEqual(tool_schema["properties"]["arguments"]["type"], "object")


if __name__ == "__main__":
    unittest.main()

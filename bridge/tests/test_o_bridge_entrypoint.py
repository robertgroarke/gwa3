"""Bridge entrypoint unit tests."""

import unittest

from bridge.__main__ import should_enable_chat


class _FakeStdin:
    def __init__(self, tty: bool):
        self._tty = tty

    def isatty(self) -> bool:
        return self._tty


class TestBridgeEntrypoint(unittest.TestCase):
    def test_no_chat_for_redirected_stdin(self):
        self.assertFalse(should_enable_chat(_FakeStdin(False)))

    def test_chat_for_interactive_stdin(self):
        self.assertTrue(should_enable_chat(_FakeStdin(True)))

    def test_no_chat_without_stdin(self):
        self.assertFalse(should_enable_chat(None))


if __name__ == "__main__":
    unittest.main()

# Error Handling Cleanup Assessment

## Scope

Reviewed all error handling patterns in `gwa3/` (C++ and Python).

## C++ (`gwa3/src/`)

### SEH (`__try`/`__except`) -- ALL KEPT

The codebase uses ~100+ `__try`/`__except` blocks across managers, bot logic, tests,
and snapshot building. These are Windows Structured Exception Handling guards around
**game memory reads** -- dereferencing pointers into the Guild Wars client's address
space that can become invalid at any time (map transitions, client updates, race
conditions with the game thread).

This is correct and necessary. Every `__try` block in this codebase guards access to
external game state through raw pointers. Removing any of them would cause crashes.

### C++ `try`/`catch` -- ALL KEPT

Only 3 instances of C++ `try`/`catch`:

1. **`BotFramework.cpp:83`** -- `catch(...)` around state handler invocation.
   Converts any exception to `BotState::Error`. Appropriate: prevents a bug in one
   state handler from crashing the entire bot thread.

2. **`LlmBridge.cpp:66`** -- `catch(const std::exception&)` around JSON parse of
   inbound IPC messages. Appropriate: external input boundary (Python bridge sends
   JSON over named pipe).

3. **`ActionExecutor.cpp:740`** -- `catch(...)` around `json::parse` of action params.
   Appropriate: parsing external input from IPC. Returns a clear error to the caller.

## Python (`gwa3/bridge/`)

### Changes Made

| File | Line | Before | After | Reason |
|------|------|--------|-------|--------|
| `agent_loop.py` | 141 | `except (asyncio.TimeoutError, Exception): pass` | `except asyncio.TimeoutError: pass` | `Exception` subsumes `TimeoutError` and silently hid real errors (pipe failures, JSON errors). Only timeouts are expected here. |
| `llm_client.py` | 20-23 | `parsed_arguments` catches `JSONDecodeError`/`TypeError`, returns `{}` | Raises on parse failure | Malformed LLM JSON output is a bug that should propagate, not be silently treated as empty params. The caller's main loop catches and logs it. |
| `llm_client.py` | 4 | `import asyncio` | removed | Unused import (exposed by the above change). |
| `ipc_client.py` | 95 | `except Exception` | `except (IOError, OSError, pywintypes.error)` | Bare `Exception` hid JSON decode errors (bugs in the C++ sender). Only pipe I/O errors should be treated as disconnect. |
| `base.py` | 61 | `except (asyncio.TimeoutError, Exception): break` | `except asyncio.TimeoutError: break` | `Exception` subsumes `TimeoutError`. The drain loop should only break on timeout, not silently swallow pipe errors. |

### Patterns Reviewed and Kept

- **`ipc_client.py:51-54`** -- `except Exception: pass` in `disconnect()`. Appropriate:
  cleanup code that must not raise regardless of pipe state.

- **`ipc_client.py:29-45`** -- `except pywintypes.error` in connect retry loop.
  Appropriate: pipe may not exist yet during startup.

- **`agent_loop.py:194-198`** -- `except (asyncio.TimeoutError, httpx.HTTPError, OSError, ValueError)`.
  Appropriate: specific exception types for an HTTP call to an external service.

- **`agent_loop.py:311-315`** -- `except asyncio.CancelledError: break` / `except Exception`.
  Appropriate: long-running loop must survive transient errors.

- **`kamadan_client.py`** -- WS-to-HTTP fallback and `except Exception` in HTTP clients.
  Appropriate: external service boundary with two independent providers. Errors are
  captured in `SearchResult.errors` and returned to caller.

- **`runner.py:80-83`** -- `except Exception: pass` in tearDown. Appropriate: test
  cleanup must not mask the actual test result.

- **`trade_harness.py`** -- Various `except OSError: pass` for file cleanup, `except Exception: return []`
  for process enumeration. Appropriate: defensive code around OS/process operations that
  are inherently unreliable.

- **All `kamadan_client.py` HTML parsing** -- `except ValueError` for timestamp parsing.
  Appropriate: external HTML data may be malformed.

## Summary

4 files changed, 5 error handling issues fixed. The changes narrow overly-broad
exception handlers to catch only expected error types, and remove one silent error
swallower that was hiding malformed LLM output as empty parameters.

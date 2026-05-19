# Bridge Test Execution Guide

## Offline Contract Tests

Run public-safe bridge contract tests from the repository root:

```powershell
python -m unittest bridge.tests.test_o_protocol_contract bridge.tests.test_p_ipc_backpressure bridge.tests.test_q_handshake bridge.tests.test_r_action_coverage
```

These tests do not require a live Guild Wars client unless explicitly enabled by environment variable.

## Live Action Coverage

`bridge.tests.test_r_action_coverage` includes an optional live dispatch probe. It requires an isolated injected client and a matching named pipe:

```powershell
$env:GWA3_PIPE_NAME='\\.\pipe\gwa3_llm_<lane>'
$env:GWA3_LIVE_ACTION_COVERAGE='1'
python -m unittest bridge.tests.test_r_action_coverage
Remove-Item Env:\GWA3_LIVE_ACTION_COVERAGE
```

The live probe sends `{}` to schema tools whose missing-parameter path is expected to be safe and verifies each one returns an `action_result` other than `unknown_action`. Tools handled locally by the Python bridge and no-parameter tools with live side effects are covered by static registration checks instead.

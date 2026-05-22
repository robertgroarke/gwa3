# GWA3 LLM Bridge

The bridge connects an injected `gwa3.dll` to an OpenAI-compatible local LLM endpoint. The DLL publishes game snapshots over a named pipe, and the Python bridge forwards model tool calls back to GWA3 actions.

## Setup

```powershell
cd bridge
pip install -r requirements.txt
```

Run your LLM server separately. The bridge defaults to Ollama's OpenAI-compatible endpoint at `http://localhost:11434/v1`, but accepts any OpenAI-compatible endpoint.

## Build And Inject

From the repository root:

```powershell
cmake --preset vs2022
cmake --build --preset vs2022 --target gwa3 injector
.\build\bin\Release\injector.exe --pid <GW_PID> --dll gwa3.dll --llm
```

Use an explicit `--pid`. In multi-client environments, do not rely on auto-selection.

## Run

```powershell
python -m bridge `
  --pipe \\.\pipe\gwa3_llm `
  --profile qwen-safe `
  --llm-url http://localhost:11434/v1 `
  --objective "Farm continuously, sell when inventory is full, and restock before the next run."
```

## Launch Profiles

The default profile is `qwen-safe`. It keeps deterministic route control in the native Froggy supervisor, uses the executor as a zero-LLM health checker, and wakes the planner only for ambiguity, recovery, unexpected state, repeated failure, or user chat.

| Profile | Intended Use |
|---------|--------------|
| `qwen-safe` | Recommended operator default: deterministic supervisor, async qwen planner, health-check executor, cached/delta prompts, strict replans |
| `qwen-deterministic` | Minimal LLM usage: qwen only for explicit chat or severe unknown/recovery states |
| `qwen-hybrid` | Native route keeps moving while async qwen plans may advise; stale planner output is discarded |
| `qwen-experimental-two-model` | Research/benchmark path using qwen planner + qwen executor |
| `spark-nano` | Future intended cloud path using `gpt-5.3-spark` planner + `gpt-5.3-nano` executor; auth/quota failures are surfaced, not silently downgraded |
| `legacy-single-model` | Existing single-model advisory baseline for comparison and rollback |

Profile defaults are composable policy switches. Explicit CLI flags such as
`--supervisor-mode`, `--executor-mode`, `--planner-mode`, `--prompt-mode`,
`--replan-policy`, `--planner-model`, and `--executor-model` override the
selected profile for experiments.

## Main Options

| Flag | Default | Description |
|------|---------|-------------|
| `--profile` | `qwen-safe` | Launch profile |
| `--llm-url` | `http://localhost:11434/v1` | OpenAI-compatible API endpoint |
| `--model` | `qwen3.5:cloud` | Single-model compatibility model name |
| `--planner-model` | profile default | Planner model |
| `--executor-model` | profile default | Executor model |
| `--supervisor-mode` | profile default | `deterministic`, `hybrid`, or `llm` |
| `--executor-mode` | profile default | `health-check`, `llm`, or `disabled` |
| `--planner-mode` | profile default | `sync` or `async` |
| `--prompt-mode` | profile default | `full`, `cached`, or `delta` |
| `--replan-policy` | profile default | `strict`, `balanced`, or `aggressive` |
| `--objective` | Generic farming | Standing objective for the agent |
| `--autonomy` | `advisory` | `advisory`, `tactical`, or `full` |
| `--pipe` | `\\.\pipe\gwa3_llm` | Named pipe exposed by the DLL |
| `--kamadan-timeout` | `10.0` | Per-source timeout for Kamadan searches |
| `--kamadan-cache-ttl` | `120.0` | Cache TTL for Kamadan search results |
| `--llm-hourly-token-cap` | `10000000` | Hard rolling LLM token cap |
| `--allow-remote-llm` | off | Required for non-local endpoints and `codex-exec` |

Remote/cloud LLM backends are disabled unless `--allow-remote-llm` is supplied.
The bridge tracks provider usage metadata and stops the agent if the rolling
hourly token cap is exceeded. During sustained token spikes, prompts are
temporarily restricted to Tier1/core state to reduce context size before the
hard cap is reached.

## Notes

Private live tests, account launch helpers, and character-specific bridge harnesses are intentionally not part of the public repository.

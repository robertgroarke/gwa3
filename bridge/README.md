# GWA3 LLM Bridge

The bridge connects an injected `gwa3.dll` to an OpenAI-compatible local LLM endpoint. The DLL publishes game snapshots over a named pipe, and the Python bridge forwards model tool calls back to GWA3 actions.

## Setup

```powershell
cd bridge
pip install -r requirements.txt
```

Run your LLM server separately. The bridge defaults to `http://localhost:8000/v1`, but accepts any OpenAI-compatible endpoint.

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
  --llm-url http://localhost:8000/v1 `
  --model <model-name> `
  --objective "Farm continuously, sell when inventory is full, and restock before the next run."
```

## Main Options

| Flag | Default | Description |
|------|---------|-------------|
| `--llm-url` | `http://localhost:8000/v1` | OpenAI-compatible API endpoint |
| `--model` | `gemma-4-32b-it` | Model name |
| `--objective` | Generic farming | Standing objective for the agent |
| `--autonomy` | `tactical` | `advisory`, `tactical`, or `full` |
| `--pipe` | `\\.\pipe\gwa3_llm` | Named pipe exposed by the DLL |
| `--kamadan-timeout` | `10.0` | Per-source timeout for Kamadan searches |
| `--kamadan-cache-ttl` | `120.0` | Cache TTL for Kamadan search results |

## Notes

Private live tests, account launch helpers, and character-specific bridge harnesses are intentionally not part of the public repository.

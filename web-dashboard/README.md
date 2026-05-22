# GWA3 Web Dashboard

The web dashboard is the browser front-end for the same local bridge contract used by the WPF Control Panel.

## LLM Mode Panels

The dashboard exposes the same operator panels and names as the desktop UI:

- Lane Launcher
- Live Chat Panel
- Plan + State Panel
- Run History Panel
- Degradation banner

For this iteration the LLM lane is BEASTRIT only. The dashboard proxies `/api/llm/*` to the bridge instead of duplicating launch or agent logic.

The default low-quota path is the bridge profile `qwen-safe`: deterministic Froggy supervision owns normal route phases, Qwen handles ambiguity/recovery/chat, the executor is health-check only, and planner prompts use delta/cached context. The Lane Launcher shows the active profile plus supervisor, executor, planner, and prompt modes from `/api/llm/state`; the dashboard does not keep separate profile logic.

Additional bridge profiles can be selected from the bridge CLI for comparison:

- `qwen-deterministic`
- `qwen-hybrid`
- `qwen-experimental-two-model`
- `spark-nano`
- `legacy-single-model`

Spark/Nano remains an explicit manual override for future readiness testing only. If a configured model or credential check fails, Launch reports `degraded` and the Degradation banner shows the preflight reason.

## Running

```powershell
cd web-dashboard
npm install
npm run start
```

The bridge URL defaults to `http://127.0.0.1:8765`. Override with `LLM_BRIDGE_URL` when testing against a stub bridge.

The bridge must be started from an elevated shell for real launch because the BEASTRIT AutoIt helper uses the validated GWLauncher path. Launch in either UI posts `{ "lane": "beastrit" }` to the same bridge endpoint; any other lane is rejected as `lane_not_permitted`.

## Desktop vs Web Parity

Identical:

- BEASTRIT lane-only launch/stop controls
- active profile and policy-mode labels
- snapshot freshness and native-helper-active labels
- chat send path through `POST /api/llm/chat`
- `chat.user` echo from either frontend to all connected observers
- assistant replies through `chat.assistant`
- role-labeled tool calls from SSE
- Plan fields and trimmed snapshot labels
- run summaries as collapsible history cards
- degradation/reconnect status surface

Intentional divergence:

- The desktop UI remains the canonical visual language and panel ordering.
- The web dashboard stays a thin browser proxy; it never shells out to AutoIt or injector directly. Exact-PID GWLauncher/injector orchestration belongs to the bridge HTTP launch endpoint.

## Screenshots

Current web LLM panel screenshot:

- `tests/artifacts/llm_dashboard_stub.png`

Current desktop screenshots are captured from the WPF production-ready run set under `C:\Users\Robert\Documents\gwa3-private\ui\runs\latest\screenshots`. Refresh both screenshot sets after the first fully captured live session for the selected model pairing so the images show real `plan.updated`, `tool.call`, and `run.summary` traffic rather than the stub stream.

## Tests

```powershell
npm run test:web
npm run test:parity
```

`test:web` runs Playwright against a stub bridge and verifies launch, chat, role-labeled tool-call rendering, and stop. `test:parity` checks the web labels against the WPF parity fixture.

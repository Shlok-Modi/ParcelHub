# ParcelHub

Priority parcel dispatch and routing engine (DAA project).

## Status
- **Phase 1** - graph, Binary Heap, Dijkstra, dispatch engine, terminal demo.
- **Phase 2** - Binomial Heap, Fibonacci Heap, heap factory, hub-queue merging, cross-heap tests.
- **Phase 3** - Prim's algorithm (minimum spanning tree/forest) on any heap, graph component counting, routing edge-case tests.
- **Phase 4** - benchmark engine: seeded graph and parcel generators, timers, operation counters, repeated runs with statistics, CSV/JSON export, and a correctness check on every run.
- **Phase 5** - Express backend (`backend/`) that runs the C++ code safely and serves JSON to the frontend.

## Build and test

Windows PowerShell (needs g++ 9 or newer, run from this folder):

    .\build.ps1
    .\build\parcelhub.exe              # Binary Heap
    .\build\parcelhub.exe binomial
    .\build\parcelhub.exe fibonacci
    .\build\parcelhub_bench.exe --profile quick     # benchmarks, see below

CMake (Linux/macOS/Windows):

    cd algorithms
    cmake -S . -B build
    cmake --build build
    ctest --test-dir build --output-on-failure

## Backend API (Phase 5)

    .\build.ps1                    # also builds build\parcelhub_api.exe
    cd backend
    npm install
    npm start                      # http://127.0.0.1:3001
    npm test                       # 70+ tests: validation, runner, API, hostile input to the C++ program

Needs Node 20 or newer. The server listens on 127.0.0.1 only unless you set `HOST`. Settings are environment variables (all optional):
`PORT`, `HOST`, `PARCELHUB_API_BIN` (path to `parcelhub_api`), `PARCELHUB_RESULTS_DIR`, `CORS_ORIGINS` (comma-separated, default `http://localhost:5173,http://localhost:3000`),
`CALC_TIMEOUT_MS`, `CALC_MAX_CONCURRENT`, `CALC_MAX_QUEUE`, `MAX_OUTPUT_BYTES`, `BENCH_TIMEOUT_MS`, `RATE_LIMIT_MAX`, `RATE_LIMIT_WINDOW_MS`, `BENCH_RATE_LIMIT_MAX`, `BENCH_RATE_LIMIT_WINDOW_MS`.

| Endpoint | Purpose |
|----------|---------|
| `GET /api/health` | 200 if the C++ program exists, otherwise 503 |
| `GET /api/sample` | the demo city, parcels and upgrade, ready to post back |
| `POST /api/route` | Dijkstra: `{heap, graph, source, destination?}` -> distances (null = unreachable), path, cost, heap operation counts |
| `POST /api/mst` | Prim: `{heap, graph, start?}` -> edges, total weight, components, `isSpanningTree` (false = forest) |
| `POST /api/dispatch` | `{heap, graph, parcels, upgrades?}` -> parcels in dispatch order, each with route and cost |
| `GET /api/benchmark/results[?format=csv]` | the files written by `parcelhub_bench` |
| `POST /api/benchmark/run` | runs a small benchmark now (one at a time) and returns the verified report |

`heap` is `binary`, `binomial`, `fibonacci` or `all`. With `all` the request runs on the three heaps and the answer is `{identical, results: [...]}`, where `identical` says whether all heaps gave the same answer (they must).
`graph` is `{nodes, edges: [{from, to, weight}]}` with hubs numbered from 1. A parcel is `{id, source, destination, level: "express"|"same-day"|"standard", deadline: "HH:MM", arrival?}`; an upgrade is `{id, level}` and must be more urgent than the parcel's current level.
Errors are always `{"error": {"code", "message"}}`: 400 invalid request, 413 body too large, 415 not JSON, 429 rate limited, 409/503 busy, 502/504 calculation stopped, 503 program missing.

Example:

    curl -s localhost:3001/api/sample > sample.json
    curl -s -X POST localhost:3001/api/route -H "content-type: application/json" -d '{"heap":"all","graph":{"nodes":3,"edges":[{"from":1,"to":2,"weight":4},{"from":2,"to":3,"weight":1}]},"source":1,"destination":3}'

How the C++ code is run safely:
- **No client text reaches a command line or a shell.** The backend starts `parcelhub_api` with a fixed (empty) argument list and `shell: false`. A validated request is turned into plain numbers and fixed words and sent through standard input.
- **Strict validation.** Unknown fields, non-integers, out-of-range numbers, duplicate ids, self-loops and invalid upgrades are rejected with a 400 before anything runs. Limits: 200 hubs, 2000 roads, 500 parcels, weights up to 1,000,000, bodies up to 200 kB.
- **The C++ program does not trust the backend.** It re-checks every number and caps every size itself (and refuses workloads that would run for minutes), so it is safe even if called directly. This is tested with hostile input, also under AddressSanitizer.
- **Resource limits.** Each run has a timeout (10 s, benchmarks 120 s) after which the process is killed, a cap on output size, at most 4 runs at once (64 waiting), and only one benchmark at a time. Rate limits apply per client (120 requests/min; 3 benchmark runs per 10 min).
- **Minimal environment.** The child only receives `PATH`, `SystemRoot`, `TEMP` and `TMP`, so secrets in the server's environment never reach it.
- **No leaks.** The program's error output and file paths are logged on the server, never sent to the client. CORS only allows the listed origins.
- The server is meant to run behind your own deployment's HTTPS and authentication if it is ever exposed beyond localhost; it has none of its own.

If `parcelhub_api.exe` fails to start on Windows with a DLL error, make sure the MinGW `bin` folder is on your `PATH` (the server passes `PATH` through).

## Benchmarks (Phase 4)

    .\build\parcelhub_bench.exe --profile quick       # seconds; for checking everything works
    .\build\parcelhub_bench.exe --profile standard    # about a minute; use this for the report
    .\build\parcelhub_bench.exe --help

(CMake builds the same tool as `parcelhub_bench`.) Results go to `results/benchmark_results.csv` and `.json`; a summary table is printed too.
Always benchmark an optimised build (`build.ps1` uses `-O2`); the tool warns if it was built without optimisation.

What is measured (each for Binary, Binomial and Fibonacci heaps):

| Benchmark | Workload | Timed part |
|-----------|----------|------------|
| `dijkstra` | single-source shortest paths on sparse (m = 2n), medium (m = 8n) and dense (m = n(n-1)/4) connected graphs | the `dijkstra()` call |
| `prim` | minimum spanning tree on the same graphs | the `prim()` call |
| `dispatch` | insert N parcels, apply 20% priority upgrades (decrease-key), extract all | all three phases, heap only (routing excluded) |
| `merge` | merge two hub queues of N/2 entries each | the `mergeFrom()` call only |

How the numbers are kept honest:
- **Reproducible.** Every input comes from a seed (`--seed`). The generator is a small splitmix64 implementation, so the same seed gives the same graphs and parcels on Windows, Linux and macOS (the C++ standard library's distributions do not guarantee that). Operation counts are identical on every run of the same seed; only the timings vary.
- **Only the algorithm is timed.** Graph and workload generation, heap construction, reference solutions and result checking all happen outside the timers (`std::chrono::steady_clock`, reported in milliseconds).
- **Repeated.** Each series does `--warmups` untimed runs and `--repeats` timed runs and reports mean, median, min, max and sample standard deviation. Use the median for tables, since one slow run does not move it. `--instances` repeats the whole thing on independent random inputs. The heaps run in a rotating order each repeat so none always goes first.
- **Verified on every run.** Dijkstra distances and predecessors are checked against a `std::priority_queue` reference, Prim against Kruskal (weight, component count, acyclic, real roads only), dispatch and merge against a plain sort. A series with any wrong result is dropped from the export, listed under `failures`, and the tool exits with code 1. Operation counts must also be identical on every repeat.
- Dense graphs above `--max-dense` hubs (default 2000) are skipped to keep memory reasonable.

CSV columns: `benchmark, heap, density, size, edges, upgrades, instance, seed, repeats, mean_ms, median_ms, min_ms, max_ms, stddev_ms, inserts, extracts, decrease_keys, merges, comparisons, verified`.
`size` is hubs (dijkstra/prim) or parcels / heap entries (dispatch/merge). The JSON has the same data plus a `meta` block (seed, repeats, compiler, optimised build, timestamp) for the report's method section.
For `merge`, the counters cover the merge step only.

Reading the results: a lower comparison count does not always mean a lower time. The heaps differ in memory layout and bookkeeping, and the Binomial and Fibonacci heaps also have to merge their id-to-node tables when merging. Report both the timing and the comparison counts, and explain differences using the table above.

## Frontend (Phase 6)

A React app (Vite) in `frontend/` with three pages: **Routes** (city map, shortest route, minimum spanning tree), **Dispatch** (run the dispatch, send parcels out one at a time, add and upgrade parcels) and **Benchmarks** (charts and a table from the saved benchmark results, or a file you open).

```powershell
# terminal 1: the calculation server
cd backend; npm install; npm start
# terminal 2: the app (http://localhost:5173, /api is proxied to the server)
cd frontend; npm install; npm run dev
```

- `npm test` runs the frontend tests (vitest); `npm run build` writes `frontend/dist`.
- After `npm run build`, the backend serves `frontend/dist` itself, so `npm start` alone is enough at http://127.0.0.1:3001. Set `FRONTEND_DIST` to use another folder. Without a build, `/` shows the JSON endpoint list.
- The city (hubs, roads, parcels) is edited in the page, checked against the same limits as the server, and kept in the browser's local storage.
- Charts are drawn with plain SVG: light and dark themes follow the system, every chart has a legend, hovering or using the arrow keys reads exact values, and each benchmark has a table view as well. Heaps use blue, orange and green plus a different marker shape each, so colour is never the only cue.

## Design decisions
- Hubs are numbered 1..n and printed as H1..Hn.
- Dispatch key = (priority, arrival, parcelId). Express=1, Same-day=2, Standard=3; earlier arrival wins ties, parcel id breaks arrival ties. Every heap therefore yields exactly the same order.
- `PriorityQueue` is the common interface. Dijkstra and DispatchEngine take it, so any heap plugs in.
- `mergeFrom()` checks the heap type at runtime and throws on a mismatch; typed `merge()` / `meld()` are also available. A rejected merge changes nothing.
- Dijkstra uses decrease-key (not lazy deletion) so the heaps are compared like for like.
- Upgrades must raise urgency; downgrades and same-level upgrades are rejected.
- Prim on a disconnected graph returns a minimum spanning FOREST: `isSpanningTree` is false and `components` gives the tree count. It is never presented as a single MST.
- Zero-weight edges and parallel edges are supported (the cheapest parallel edge wins). Negative weights and self-loops are rejected when the edge is added.

## Complexity notes
| Operation    | Binary   | Binomial | Fibonacci (amortised) |
|--------------|----------|----------|-----------------------|
| insert       | O(log n) | O(log n) | O(1)                  |
| extract-min  | O(log n) | O(log n) | O(log n)              |
| decrease-key | O(log n) | O(log n) | O(1)                  |
| merge        | O(n)     | O(log n) | O(1)                  |

The merge costs are for the heap structure. Each heap also keeps an id -> node table (needed for decrease-key and duplicate detection), and merging that table costs O(m) for the smaller heap's m items. Mention this in the report and benchmark merges honestly.

## Tests
- `heapTests` - identical behavioural suite on all three heaps, plus Binomial (root count = popcount(n)) and Fibonacci (cuts, cascading cuts, meld) structure checks. Every heap has `checkInvariants()`.
- `crossHeapTests` - 5 seeds x 15,000 random insert/extract/decrease-key/merge operations on all three heaps against a reference model; Dijkstra and dispatch results must match across heaps.
- `primTests` - hand-checked MST (weight 36 on the sample graph), edge cases (single hub, no edges, zero-weight, parallel edges, all-equal weights), and 120 random graphs checked against an independent Kruskal implementation: minimum weight, acyclic, every edge is a real road, forest spans exactly the graph's components, identical edges on all three heaps and any start hub.
- `routingTests` - Dijkstra against hand-checked distances and a reference, plus edge cases.
- `dispatchTests` - dispatch order, upgrades, validation.
- `backend/test` - Node tests (`npm test`): request validation, the process runner (timeout, output cap, no shell, no environment leak, concurrency), the HTTP API against the real C++ program (results compared with an independent JavaScript Dijkstra and Kruskal on 100 random graphs), error mapping, CORS, rate limits, and direct hostile input to the C++ program.
- `benchmarkTests` - generators (determinism, connectivity, density, golden RNG values), statistics, reference solutions, the result verifiers, the runner (verified rows, reproducibility, option handling), fault injection (a deliberately wrong heap must be rejected and never exported), timer scope (a slow heap constructor must not show up in the timings), heap-order rotation, and CSV/JSON export.

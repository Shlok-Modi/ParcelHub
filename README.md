# ParcelHub

Priority parcel dispatch and routing engine (DAA project).

## Status
- **Phase 1** - graph, Binary Heap, Dijkstra, dispatch engine, terminal demo.
- **Phase 2** - Binomial Heap, Fibonacci Heap, heap factory, hub-queue merging, cross-heap tests.
- **Phase 3** - Prim's algorithm (minimum spanning tree/forest) on any heap, graph component counting, routing edge-case tests.
- **Phase 4** - benchmark engine: seeded graph and parcel generators, timers, operation counters, repeated runs with statistics, CSV/JSON export, and a correctness check on every run.

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
- `benchmarkTests` - generators (determinism, connectivity, density, golden RNG values), statistics, reference solutions, the result verifiers, the runner (verified rows, reproducibility, option handling), fault injection (a deliberately wrong heap must be rejected and never exported), timer scope (a slow heap constructor must not show up in the timings), heap-order rotation, and CSV/JSON export.

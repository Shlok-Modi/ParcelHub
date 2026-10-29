# ParcelHub

Priority parcel dispatch and routing engine (DAA project).

## Status
- **Phase 1** - graph, Binary Heap, Dijkstra, dispatch engine, terminal demo.
- **Phase 2** - Binomial Heap, Fibonacci Heap, heap factory, hub-queue merging, cross-heap tests.

## Build and test

Windows PowerShell (needs g++ 9 or newer, run from this folder):

    .\build.ps1
    .\build\parcelhub.exe              # Binary Heap
    .\build\parcelhub.exe binomial
    .\build\parcelhub.exe fibonacci

CMake (Linux/macOS/Windows):

    cd algorithms
    cmake -S . -B build
    cmake --build build
    ctest --test-dir build --output-on-failure

## Design decisions
- Hubs are numbered 1..n and printed as H1..Hn.
- Dispatch key = (priority, arrival, parcelId). Express=1, Same-day=2, Standard=3; earlier arrival wins ties, parcel id breaks arrival ties. Every heap therefore yields exactly the same order.
- `PriorityQueue` is the common interface. Dijkstra and DispatchEngine take it, so any heap plugs in.
- `mergeFrom()` checks the heap type at runtime and throws on a mismatch; typed `merge()` / `meld()` are also available. A rejected merge changes nothing.
- Dijkstra uses decrease-key (not lazy deletion) so the heaps are compared like for like.
- Upgrades must raise urgency; downgrades and same-level upgrades are rejected.

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
- `routingTests`, `dispatchTests` - Phase 1 suites.

# ParcelHub - Phase 1 (core baseline)

Terminal prototype: weighted graph, Binary Heap (insert / extract-min / decrease-key / merge),
Dijkstra with path reconstruction, and a priority-based dispatch engine.

## Build & run (CMake)
    cd algorithms
    cmake -S . -B build && cmake --build build
    ./build/parcelhub          # demo
    cd build && ctest          # tests

## Build without CMake
    g++ -std=c++17 -O2 -Ialgorithms/include algorithms/src/{Graph,BinaryHeap,Dijkstra,DispatchEngine,main}.cpp -o parcelhub

## Design decisions
- Hubs are numbered 1..n and printed as H1..Hn.
- Dispatch key = (priority, arrival, parcelId): Express=1, Same-day=2, Standard=3; earlier arrival wins ties, id breaks arrival ties.
- `PriorityQueue` interface is what Binomial/Fibonacci heaps will implement in Phase 2; Dijkstra and DispatchEngine take it by interface.
- `merge` lives on each concrete heap, not on the interface, so heaps of different types can't be merged.
- Dijkstra uses decrease-key (not lazy deletion) so heap comparisons in Phase 4 are like for like.
- Upgrades must raise urgency; downgrades and re-upgrades to the same level are rejected.

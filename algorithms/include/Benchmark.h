#pragma once
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>
#include "HeapFactory.h"
#include "PriorityQueue.h"
#include "Stats.h"
#include "Workload.h"

// Creates the heap under test. Defaults to makeHeap; tests substitute a faulty heap
// to prove that wrong results are caught.
using HeapProvider = std::function<std::unique_ptr<PriorityQueue>(HeapType)>;

struct BenchConfig {
    std::string profile = "custom";
    uint64_t seed = 20240601;
    int repeats = 5;        // timed runs per (case, heap)
    int warmups = 1;        // untimed runs first (still verified)
    int instances = 3;      // independent random inputs per size
    long long maxWeight = 1000;

    std::vector<HeapType> heaps{std::begin(kAllHeapTypes), std::end(kAllHeapTypes)};
    std::vector<GraphDensity> densities{std::begin(kAllDensities), std::end(kAllDensities)};
    std::vector<int> graphSizes{100, 500, 1000};      // hubs, for Dijkstra and Prim
    int maxDenseNodes = 2000;                         // dense graphs above this are skipped (memory)
    std::vector<int> parcelSizes{1000, 10000};        // parcels (dispatch) / entries (merge)
    double upgradeFraction = 0.2;
    int parcelHubs = 100;

    bool runDijkstra = true, runPrim = true, runDispatch = true, runMerge = true;

    HeapProvider provider = [](HeapType t) { return makeHeap(t); };
};

// "quick" (seconds) or "standard" (a few minutes). Throws invalid_argument otherwise.
BenchConfig profileConfig(const std::string& name);

struct BenchMeta {
    std::string profile, compiler, timestampUtc;
    bool optimized = false;
    uint64_t seed = 0;
    int repeats = 0, warmups = 0, instances = 0;
    long long maxWeight = 0;
    double upgradeFraction = 0;
};

// One verified measurement series: a single (benchmark, input instance, heap).
struct BenchRow {
    std::string benchmark;      // "dijkstra" | "prim" | "dispatch" | "merge"
    std::string heap;           // e.g. "Fibonacci Heap"
    std::string density;        // graph benchmarks only, otherwise empty
    int size = 0;               // hubs (graph benchmarks) or parcels / heap entries
    long long edges = -1;       // graph benchmarks only
    long long upgrades = -1;    // dispatch only
    int instance = 0;
    uint64_t seed = 0;          // seed of this instance's input
    int repeats = 0;
    Stats timeMs;               // wall-clock time of the algorithm call only
    OpCounters ops;             // heap operations in that call (identical every repeat)
};

// A series that was dropped because a result was wrong (or counts were not repeatable).
struct BenchFailure {
    std::string benchmark, heap, density;
    int size = 0, instance = 0;
    uint64_t seed = 0;
    std::string message;
};

struct BenchReport {
    BenchMeta meta;
    std::vector<BenchRow> rows;
    std::vector<BenchFailure> failures;
    bool ok() const { return failures.empty(); }
};

// Runs every enabled benchmark. Input generation, heap construction, reference solutions
// and result verification all happen outside the timed region. Every run (warm-up and
// timed) is verified; a series with any wrong result is reported as a failure and its
// timings are not exported. Throws invalid_argument for an invalid configuration.
BenchReport runBenchmarks(const BenchConfig& cfg, std::ostream* progress = nullptr);

// Human-readable summary: per (benchmark, density, size) the median time of each heap
// averaged over instances, comparison counts, and the ratio to the Binary Heap.
void printSummary(const BenchReport& report, std::ostream& out);

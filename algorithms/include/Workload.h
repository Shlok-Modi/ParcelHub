#pragma once
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>
#include "Graph.h"
#include "Parcel.h"
#include "PriorityQueue.h"

// Small deterministic generator (splitmix64). Unlike std::uniform_int_distribution,
// its output is identical on every compiler and OS, so a seed reproduces the same
// workload on Windows, Linux and macOS.
struct Rng {
    uint64_t state;
    explicit Rng(uint64_t seed) : state(seed) {}
    uint64_t next() {
        uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    uint64_t below(uint64_t n) { return next() % n; }                     // n must be > 0
    long long range(long long lo, long long hi) { return lo + static_cast<long long>(below(static_cast<uint64_t>(hi - lo + 1))); }
};

// Derives an independent, reproducible seed for one benchmark case.
// Result fits in 53 bits so it survives a round trip through JSON numbers.
uint64_t mixSeed(uint64_t base, std::initializer_list<uint64_t> parts);

// ---- City graphs -------------------------------------------------------------

enum class GraphDensity { Sparse, Medium, Dense };
inline const GraphDensity kAllDensities[] = {GraphDensity::Sparse, GraphDensity::Medium, GraphDensity::Dense};
const char* toString(GraphDensity d);                           // "sparse" | "medium" | "dense"
std::optional<GraphDensity> parseDensity(const std::string& name);

// Edge count the generator aims for: sparse = 2n, medium = 8n, dense = n(n-1)/4,
// never below n-1 (so the graph can be connected) and never above n(n-1)/2.
std::size_t targetEdgeCount(int nodes, GraphDensity d);

// Connected, simple (no self-loops, no parallel edges) undirected graph with integer
// weights in [1, maxWeight]. Built from a random spanning tree plus random extra
// roads, so the same (nodes, density, seed, maxWeight) always gives the same graph.
Graph generateCityGraph(int nodes, GraphDensity d, uint64_t seed, long long maxWeight = 1000);

// ---- Parcel workloads --------------------------------------------------------

struct ParcelSpec {
    int id = 0;                  // ids are 1..N, equal to arrival + 1
    int source = 0, destination = 0;
    ServiceLevel level = ServiceLevel::Standard;
    int deadlineMinutes = 0;
    int arrival = 0;
};

struct UpgradeSpec {
    int parcelId = 0;
    ServiceLevel newLevel = ServiceLevel::Express;   // always strictly more urgent than the parcel's level
};

struct ParcelWorkload {
    std::vector<ParcelSpec> parcels;
    std::vector<UpgradeSpec> upgrades;   // each parcel appears at most once; applied in this order
};

// ~15% Express, ~25% Same-day, ~60% Standard. `upgradeFraction` of all parcels (capped by
// how many are not already Express) get one genuine upgrade.
ParcelWorkload generateParcelWorkload(int hubCount, int parcelCount, double upgradeFraction, uint64_t seed);

// Dispatch key of a parcel at a given level: (priority, arrival, id).
Key dispatchKey(const ParcelSpec& p, ServiceLevel level);

// Reference answer: parcel ids in the order a correct queue must dispatch them after
// all upgrades have been applied (a plain sort, no heap involved).
std::vector<int> expectedDispatchOrder(const ParcelWorkload& w);

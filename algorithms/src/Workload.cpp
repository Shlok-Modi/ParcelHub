#include "Workload.h"
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <unordered_set>

uint64_t mixSeed(uint64_t base, std::initializer_list<uint64_t> parts) {
    uint64_t h = base;
    for (uint64_t p : parts) {
        h ^= p + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
        h = Rng(h).next();
    }
    return h & ((1ULL << 53) - 1);
}

const char* toString(GraphDensity d) {
    switch (d) {
        case GraphDensity::Sparse: return "sparse";
        case GraphDensity::Medium: return "medium";
        default:                   return "dense";
    }
}

std::optional<GraphDensity> parseDensity(const std::string& name) {
    if (name == "sparse") return GraphDensity::Sparse;
    if (name == "medium") return GraphDensity::Medium;
    if (name == "dense")  return GraphDensity::Dense;
    return std::nullopt;
}

std::size_t targetEdgeCount(int nodes, GraphDensity d) {
    if (nodes <= 1) return 0;
    const std::size_t n = static_cast<std::size_t>(nodes);
    const std::size_t maxEdges = n * (n - 1) / 2;
    std::size_t want = 0;
    switch (d) {
        case GraphDensity::Sparse: want = 2 * n; break;
        case GraphDensity::Medium: want = 8 * n; break;
        default:                   want = n * (n - 1) / 4; break;
    }
    return std::min(maxEdges, std::max(n - 1, want));
}

Graph generateCityGraph(int nodes, GraphDensity d, uint64_t seed, long long maxWeight) {
    if (maxWeight < 1) throw std::invalid_argument("generateCityGraph: maxWeight must be at least 1");
    Graph g(nodes);   // throws if nodes <= 0
    Rng rng(seed);

    auto pairKey = [](int u, int v) {
        if (u > v) std::swap(u, v);
        return (static_cast<uint64_t>(u) << 32) | static_cast<uint64_t>(v);
    };
    const std::size_t target = targetEdgeCount(nodes, d);
    std::unordered_set<uint64_t> used;
    used.reserve(target * 2 + 1);

    // Random spanning tree: shuffle the hubs, attach each one to a random earlier hub.
    std::vector<int> perm(nodes);
    std::iota(perm.begin(), perm.end(), 1);
    for (int i = nodes - 1; i > 0; --i) std::swap(perm[i], perm[rng.below(static_cast<uint64_t>(i) + 1)]);
    for (int i = 1; i < nodes; ++i) {
        int u = perm[i], v = perm[rng.below(static_cast<uint64_t>(i))];
        g.addEdge(u, v, rng.range(1, maxWeight));
        used.insert(pairKey(u, v));
    }
    // Extra roads: random distinct pairs until the target is reached.
    while (used.size() < target) {
        int u = 1 + static_cast<int>(rng.below(static_cast<uint64_t>(nodes)));
        int v = 1 + static_cast<int>(rng.below(static_cast<uint64_t>(nodes)));
        if (u == v || !used.insert(pairKey(u, v)).second) continue;
        g.addEdge(u, v, rng.range(1, maxWeight));
    }
    return g;
}

Key dispatchKey(const ParcelSpec& p, ServiceLevel level) {
    return {static_cast<long long>(level), p.arrival, p.id};
}

ParcelWorkload generateParcelWorkload(int hubCount, int parcelCount, double upgradeFraction, uint64_t seed) {
    if (hubCount <= 0) throw std::invalid_argument("generateParcelWorkload: hubCount must be positive");
    if (parcelCount < 0) throw std::invalid_argument("generateParcelWorkload: parcelCount must not be negative");
    if (upgradeFraction < 0.0 || upgradeFraction > 1.0)
        throw std::invalid_argument("generateParcelWorkload: upgradeFraction must be in [0, 1]");

    Rng rng(seed);
    ParcelWorkload w;
    w.parcels.reserve(parcelCount);
    for (int i = 0; i < parcelCount; ++i) {
        ParcelSpec p;
        p.id = i + 1;
        p.arrival = i;
        p.source = 1 + static_cast<int>(rng.below(static_cast<uint64_t>(hubCount)));
        p.destination = 1 + static_cast<int>(rng.below(static_cast<uint64_t>(hubCount)));
        if (hubCount > 1)
            while (p.destination == p.source) p.destination = 1 + static_cast<int>(rng.below(static_cast<uint64_t>(hubCount)));
        uint64_t roll = rng.below(100);
        p.level = roll < 15 ? ServiceLevel::Express : (roll < 40 ? ServiceLevel::SameDay : ServiceLevel::Standard);
        p.deadlineMinutes = 8 * 60 + static_cast<int>(rng.below(10 * 60));
        w.parcels.push_back(p);
    }

    std::vector<int> candidates;   // indices of parcels that can still be upgraded
    for (int i = 0; i < parcelCount; ++i)
        if (w.parcels[i].level != ServiceLevel::Express) candidates.push_back(i);
    for (int i = static_cast<int>(candidates.size()) - 1; i > 0; --i)
        std::swap(candidates[i], candidates[rng.below(static_cast<uint64_t>(i) + 1)]);
    std::size_t want = static_cast<std::size_t>(upgradeFraction * parcelCount);
    want = std::min(want, candidates.size());
    for (std::size_t k = 0; k < want; ++k) {
        const ParcelSpec& p = w.parcels[candidates[k]];
        UpgradeSpec u;
        u.parcelId = p.id;
        if (p.level == ServiceLevel::SameDay) u.newLevel = ServiceLevel::Express;
        else u.newLevel = rng.below(2) ? ServiceLevel::Express : ServiceLevel::SameDay;
        w.upgrades.push_back(u);
    }
    return w;
}

std::vector<int> expectedDispatchOrder(const ParcelWorkload& w) {
    std::vector<ServiceLevel> level(w.parcels.size());
    for (std::size_t i = 0; i < w.parcels.size(); ++i) level[i] = w.parcels[i].level;
    for (const UpgradeSpec& u : w.upgrades) level[u.parcelId - 1] = u.newLevel;
    std::vector<HeapEntry> entries;
    entries.reserve(w.parcels.size());
    for (std::size_t i = 0; i < w.parcels.size(); ++i)
        entries.push_back({w.parcels[i].id, dispatchKey(w.parcels[i], level[i])});
    std::sort(entries.begin(), entries.end(), [](const HeapEntry& a, const HeapEntry& b) { return a.key < b.key; });
    std::vector<int> order;
    order.reserve(entries.size());
    for (const HeapEntry& e : entries) order.push_back(e.id);
    return order;
}

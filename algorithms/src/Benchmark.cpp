#include "Benchmark.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <map>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include "Dijkstra.h"
#include "Prim.h"
#include "Reference.h"

namespace {

using Clock = std::chrono::steady_clock;

struct Sample {
    long long ns = 0;
    OpCounters ops;
};
using RunOnce = std::function<Sample(HeapType)>;   // throws runtime_error if the result is wrong

long long elapsedNs(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();
}

bool sameOps(const OpCounters& a, const OpCounters& b) {
    return a.inserts == b.inserts && a.extracts == b.extracts && a.decreaseKeys == b.decreaseKeys &&
           a.merges == b.merges && a.comparisons == b.comparisons;
}

OpCounters opsDelta(const OpCounters& after, const OpCounters& before) {
    OpCounters d;
    d.inserts = after.inserts - before.inserts;
    d.extracts = after.extracts - before.extracts;
    d.decreaseKeys = after.decreaseKeys - before.decreaseKeys;
    d.merges = after.merges - before.merges;
    d.comparisons = after.comparisons - before.comparisons;
    return d;
}

struct CaseInfo {
    std::string benchmark, density;
    int size = 0, instance = 0;
    long long edges = -1, upgrades = -1;
    uint64_t seed = 0;
};

// Runs one input against every heap. Heaps are visited in a rotating order on each
// repeat so no heap always runs first (cache/frequency effects average out).
void runCase(const BenchConfig& cfg, BenchReport& report, const CaseInfo& info, const RunOnce& run) {
    const std::size_t H = cfg.heaps.size();
    std::vector<std::vector<double>> times(H);
    std::vector<OpCounters> ops(H);
    std::vector<char> haveOps(H, 0), failed(H, 0);
    std::vector<std::string> message(H);

    const int rounds = cfg.warmups + cfg.repeats;
    for (int round = 0; round < rounds; ++round) {
        for (std::size_t k = 0; k < H; ++k) {
            std::size_t hi = (k + static_cast<std::size_t>(round)) % H;
            if (failed[hi]) continue;
            try {
                Sample s = run(cfg.heaps[hi]);
                if (!haveOps[hi]) { ops[hi] = s.ops; haveOps[hi] = 1; }
                else if (!sameOps(ops[hi], s.ops))
                    throw std::runtime_error("operation counts changed between repeats (not reproducible)");
                if (round >= cfg.warmups) times[hi].push_back(static_cast<double>(s.ns) / 1e6);
            } catch (const std::exception& e) {
                failed[hi] = 1;
                message[hi] = e.what();
            }
        }
    }

    for (std::size_t hi = 0; hi < H; ++hi) {
        std::string heapName = toString(cfg.heaps[hi]);
        if (failed[hi]) {
            BenchFailure f;
            f.benchmark = info.benchmark; f.heap = heapName; f.density = info.density;
            f.size = info.size; f.instance = info.instance; f.seed = info.seed;
            f.message = message[hi];
            report.failures.push_back(f);
            continue;
        }
        BenchRow row;
        row.benchmark = info.benchmark; row.heap = heapName; row.density = info.density;
        row.size = info.size; row.edges = info.edges; row.upgrades = info.upgrades;
        row.instance = info.instance; row.seed = info.seed; row.repeats = cfg.repeats;
        row.timeMs = computeStats(times[hi]);
        row.ops = ops[hi];
        report.rows.push_back(row);
    }
}

void note(std::ostream* progress, const std::string& text) {
    if (progress) *progress << text << std::endl;
}

void runGraphBenchmarks(const BenchConfig& cfg, BenchReport& report, std::ostream* progress, bool dijkstraPass) {
    const char* name = dijkstraPass ? "dijkstra" : "prim";
    for (GraphDensity d : cfg.densities) {
        for (int n : cfg.graphSizes) {
            if (d == GraphDensity::Dense && n > cfg.maxDenseNodes) {
                note(progress, std::string("[") + name + "] skipping dense graph with " + std::to_string(n) +
                                   " hubs (above maxDenseNodes=" + std::to_string(cfg.maxDenseNodes) + ")");
                continue;
            }
            for (int inst = 0; inst < cfg.instances; ++inst) {
                // Same seed for Dijkstra and Prim, so both see the very same graph.
                uint64_t seed = mixSeed(cfg.seed, {1, static_cast<uint64_t>(d), static_cast<uint64_t>(n), static_cast<uint64_t>(inst)});
                Graph g = generateCityGraph(n, d, seed, cfg.maxWeight);     // not timed
                CaseInfo info;
                info.benchmark = name; info.density = toString(d); info.size = n;
                info.edges = static_cast<long long>(g.edgeCount()); info.instance = inst; info.seed = seed;
                note(progress, std::string("[") + name + "] " + info.density + " n=" + std::to_string(n) +
                                   " m=" + std::to_string(g.edgeCount()) + " instance " + std::to_string(inst + 1) + "/" +
                                   std::to_string(cfg.instances));
                if (dijkstraPass) {
                    const std::vector<long long> refDist = referenceDistances(g, 1);
                    runCase(cfg, report, info, [&](HeapType h) {
                        auto pq = cfg.provider(h);                           // not timed
                        auto t0 = Clock::now();
                        DijkstraResult r = dijkstra(g, 1, *pq);
                        auto t1 = Clock::now();
                        std::string err = verifyDijkstra(g, r, refDist);     // not timed
                        if (!err.empty()) throw std::runtime_error(err);
                        return Sample{elapsedNs(t0, t1), r.heapOps};
                    });
                } else {
                    const ReferenceMst ref = referenceMst(g);
                    runCase(cfg, report, info, [&](HeapType h) {
                        auto pq = cfg.provider(h);
                        auto t0 = Clock::now();
                        PrimResult r = prim(g, 1, *pq);
                        auto t1 = Clock::now();
                        std::string err = verifyPrim(g, r, ref);
                        if (!err.empty()) throw std::runtime_error(err);
                        return Sample{elapsedNs(t0, t1), r.heapOps};
                    });
                }
            }
        }
    }
}

void runDispatchBenchmark(const BenchConfig& cfg, BenchReport& report, std::ostream* progress) {
    for (int n : cfg.parcelSizes) {
        for (int inst = 0; inst < cfg.instances; ++inst) {
            uint64_t seed = mixSeed(cfg.seed, {2, static_cast<uint64_t>(n), static_cast<uint64_t>(inst)});
            ParcelWorkload w = generateParcelWorkload(cfg.parcelHubs, n, cfg.upgradeFraction, seed);   // not timed
            const std::vector<int> expected = expectedDispatchOrder(w);
            CaseInfo info;
            info.benchmark = "dispatch"; info.size = n; info.upgrades = static_cast<long long>(w.upgrades.size());
            info.instance = inst; info.seed = seed;
            note(progress, "[dispatch] " + std::to_string(n) + " parcels, " + std::to_string(w.upgrades.size()) +
                               " upgrades, instance " + std::to_string(inst + 1) + "/" + std::to_string(cfg.instances));
            runCase(cfg, report, info, [&](HeapType h) {
                auto pq = cfg.provider(h);
                std::vector<int> order(w.parcels.size());   // result buffer, allocated before the timer
                auto t0 = Clock::now();
                for (const ParcelSpec& p : w.parcels) pq->insert(p.id, dispatchKey(p, p.level));
                for (const UpgradeSpec& u : w.upgrades)
                    pq->decreaseKey(u.parcelId, dispatchKey(w.parcels[u.parcelId - 1], u.newLevel));
                for (std::size_t i = 0; i < order.size(); ++i) order[i] = pq->extractMin().id;
                auto t1 = Clock::now();
                std::string err = verifyOrder(order, expected);
                if (!err.empty()) throw std::runtime_error(err);
                return Sample{elapsedNs(t0, t1), pq->counters()};
            });
        }
    }
}

void runMergeBenchmark(const BenchConfig& cfg, BenchReport& report, std::ostream* progress) {
    for (int n : cfg.parcelSizes) {
        for (int inst = 0; inst < cfg.instances; ++inst) {
            uint64_t seed = mixSeed(cfg.seed, {3, static_cast<uint64_t>(n), static_cast<uint64_t>(inst)});
            Rng rng(seed);
            std::vector<HeapEntry> left, right;     // two hub queues with disjoint ids
            for (int id = 1; id <= n; ++id) {
                HeapEntry e{id, Key{static_cast<long long>(rng.below(1000)), static_cast<long long>(rng.below(1000)), id}};
                (rng.below(2) ? left : right).push_back(e);
            }
            std::vector<HeapEntry> all = left;
            all.insert(all.end(), right.begin(), right.end());
            std::sort(all.begin(), all.end(), [](const HeapEntry& a, const HeapEntry& b) { return a.key < b.key; });
            std::vector<int> expected;
            for (const HeapEntry& e : all) expected.push_back(e.id);

            CaseInfo info;
            info.benchmark = "merge"; info.size = n; info.instance = inst; info.seed = seed;
            note(progress, "[merge] " + std::to_string(left.size()) + " + " + std::to_string(right.size()) +
                               " entries, instance " + std::to_string(inst + 1) + "/" + std::to_string(cfg.instances));
            runCase(cfg, report, info, [&](HeapType h) {
                auto a = cfg.provider(h);
                auto b = cfg.provider(h);
                for (const HeapEntry& e : left) a->insert(e.id, e.key);     // setup, not timed
                for (const HeapEntry& e : right) b->insert(e.id, e.key);
                const OpCounters before = a->counters();
                auto t0 = Clock::now();
                a->mergeFrom(*b);
                auto t1 = Clock::now();
                const OpCounters mergeOps = opsDelta(a->counters(), before);
                if (!b->empty() || a->size() != all.size()) throw std::runtime_error("merge left the wrong number of entries");
                std::vector<int> order;
                order.reserve(all.size());
                while (!a->empty()) order.push_back(a->extractMin().id);    // verification, not timed
                std::string err = verifyOrder(order, expected);
                if (!err.empty()) throw std::runtime_error(err);
                return Sample{elapsedNs(t0, t1), mergeOps};
            });
        }
    }
}

std::string utcNow() {
    std::time_t t = std::time(nullptr);
    std::tm tmv{};
#ifdef _WIN32
    gmtime_s(&tmv, &t);
#else
    gmtime_r(&t, &tmv);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tmv);
    return buf;
}

void validate(const BenchConfig& cfg) {
    if (cfg.repeats < 1) throw std::invalid_argument("repeats must be at least 1");
    if (cfg.warmups < 0) throw std::invalid_argument("warmups must not be negative");
    if (cfg.instances < 1) throw std::invalid_argument("instances must be at least 1");
    if (cfg.heaps.empty()) throw std::invalid_argument("at least one heap is required");
    if (cfg.maxWeight < 1) throw std::invalid_argument("maxWeight must be at least 1");
    if (cfg.upgradeFraction < 0.0 || cfg.upgradeFraction > 1.0) throw std::invalid_argument("upgradeFraction must be in [0, 1]");
    if (cfg.parcelHubs < 1) throw std::invalid_argument("parcelHubs must be at least 1");
    if (!cfg.provider) throw std::invalid_argument("heap provider is missing");
    for (int n : cfg.graphSizes) if (n < 2) throw std::invalid_argument("graph sizes must be at least 2 hubs");
    for (int n : cfg.parcelSizes) if (n < 2) throw std::invalid_argument("parcel sizes must be at least 2");
}

}  // namespace

BenchConfig profileConfig(const std::string& name) {
    BenchConfig c;
    c.profile = name;
    if (name == "quick") {
        c.repeats = 3; c.instances = 1;
        c.graphSizes = {100, 500, 1000};
        c.parcelSizes = {1000, 10000};
    } else if (name == "standard") {
        c.repeats = 5; c.instances = 3;
        c.graphSizes = {100, 500, 1000, 2000, 5000};
        c.parcelSizes = {1000, 10000, 100000};
    } else {
        throw std::invalid_argument("unknown profile '" + name + "' (use quick or standard)");
    }
    return c;
}

BenchReport runBenchmarks(const BenchConfig& cfg, std::ostream* progress) {
    validate(cfg);
    BenchReport report;
    report.meta.profile = cfg.profile;
    report.meta.seed = cfg.seed;
    report.meta.repeats = cfg.repeats;
    report.meta.warmups = cfg.warmups;
    report.meta.instances = cfg.instances;
    report.meta.maxWeight = cfg.maxWeight;
    report.meta.upgradeFraction = cfg.upgradeFraction;
    report.meta.timestampUtc = utcNow();
#if defined(__clang__)
    report.meta.compiler = std::string("clang ") + __clang_version__;
#elif defined(__GNUC__)
    report.meta.compiler = std::string("gcc ") + __VERSION__;
#elif defined(_MSC_VER)
    report.meta.compiler = "msvc " + std::to_string(_MSC_VER);
#else
    report.meta.compiler = "unknown";
#endif
#if defined(__OPTIMIZE__) || defined(NDEBUG)
    report.meta.optimized = true;
#endif

    if (cfg.runDijkstra) runGraphBenchmarks(cfg, report, progress, true);
    if (cfg.runPrim) runGraphBenchmarks(cfg, report, progress, false);
    if (cfg.runDispatch) runDispatchBenchmark(cfg, report, progress);
    if (cfg.runMerge) runMergeBenchmark(cfg, report, progress);
    return report;
}

void printSummary(const BenchReport& report, std::ostream& out) {
    struct Agg { double median = 0, stddev = 0, comparisons = 0; int n = 0; };
    struct Group {
        std::string benchmark, density;
        int size;
        std::vector<std::string> heapOrder;
        std::map<std::string, Agg> heaps;
        int instances = 0;
    };
    std::vector<Group> groups;
    for (const BenchRow& r : report.rows) {
        Group* g = nullptr;
        for (Group& x : groups)
            if (x.benchmark == r.benchmark && x.density == r.density && x.size == r.size) { g = &x; break; }
        if (!g) { groups.push_back({r.benchmark, r.density, r.size, {}, {}, 0}); g = &groups.back(); }
        if (!g->heaps.count(r.heap)) g->heapOrder.push_back(r.heap);
        Agg& a = g->heaps[r.heap];
        a.median += r.timeMs.median; a.stddev += r.timeMs.stddev; a.comparisons += static_cast<double>(r.ops.comparisons);
        ++a.n;
        g->instances = std::max(g->instances, a.n);
    }

    out << "Benchmark summary (times are the median of " << report.meta.repeats << " runs, averaged over instances; "
        << "seed " << report.meta.seed << ")\n";
    if (!report.meta.optimized)
        out << "WARNING: built without optimisation - timings are not representative. Build with -O2.\n";
    for (const Group& g : groups) {
        out << "\n== " << g.benchmark;
        if (!g.density.empty()) out << " | " << g.density << " graph | " << g.size << " hubs";
        else out << " | " << g.size << (g.benchmark == "merge" ? " entries" : " parcels");
        out << " | " << g.instances << " instance(s) ==\n";
        out << std::left << std::setw(16) << "Heap" << std::right << std::setw(14) << "median ms" << std::setw(12) << "stddev ms"
            << std::setw(16) << "comparisons" << std::setw(12) << "vs Binary" << "\n";
        double binary = 0;
        auto itB = g.heaps.find("Binary Heap");
        if (itB != g.heaps.end() && itB->second.n > 0) binary = itB->second.median / itB->second.n;
        double best = -1;
        for (const auto& kv : g.heaps) {
            double m = kv.second.median / kv.second.n;
            if (best < 0 || m < best) best = m;
        }
        for (const std::string& heap : g.heapOrder) {
            const Agg& a = g.heaps.at(heap);
            double m = a.median / a.n;
            out << std::left << std::setw(16) << heap << std::right << std::fixed << std::setprecision(4) << std::setw(14) << m
                << std::setw(12) << a.stddev / a.n << std::setprecision(0) << std::setw(16) << a.comparisons / a.n;
            if (binary > 0) out << std::setprecision(2) << std::setw(11) << m / binary << "x";
            else out << std::setw(12) << "-";
            if (m == best) out << "  <- fastest";
            out << "\n";
        }
    }
    if (!report.failures.empty()) {
        out << "\nVERIFICATION FAILURES (" << report.failures.size() << " series dropped from the results):\n";
        for (const BenchFailure& f : report.failures)
            out << "  " << f.benchmark << " | " << f.heap << " | " << f.density << " n=" << f.size << " instance " << f.instance
                << " seed " << f.seed << ": " << f.message << "\n";
    } else {
        out << "\nAll " << report.rows.size() << " measured series passed verification.\n";
    }
}

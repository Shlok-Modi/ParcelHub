// Tests for the Phase 4 benchmark engine: generators, statistics, reference solutions,
// result verification, the runner (including fault injection) and CSV/JSON export.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <thread>
#include "Benchmark.h"
#include "BenchmarkExport.h"
#include "Reference.h"
#include "TestUtil.h"
#include "Workload.h"

// ---- helpers ---------------------------------------------------------------------

static bool sameGraph(const Graph& a, const Graph& b) {
    if (a.nodeCount() != b.nodeCount() || a.edgeCount() != b.edgeCount()) return false;
    for (int u = 1; u <= a.nodeCount(); ++u) {
        const auto& x = a.neighbors(u);
        const auto& y = b.neighbors(u);
        if (x.size() != y.size()) return false;
        for (std::size_t i = 0; i < x.size(); ++i)
            if (x[i].to != y[i].to || x[i].weight != y[i].weight) return false;
    }
    return true;
}


// Correct pass-through heap (a Binary Heap behind the interface); FaultyHeap and DriftHeap build on it.
class FaultyHeapBase : public PriorityQueue {
public:
    void insert(int id, const Key& k) override { in_->insert(id, k); }
    HeapEntry extractMin() override { return in_->extractMin(); }
    const HeapEntry& peekMin() const override { return in_->peekMin(); }
    void decreaseKey(int id, const Key& k) override { in_->decreaseKey(id, k); }
    bool contains(int id) const override { return in_->contains(id); }
    bool empty() const override { return in_->empty(); }
    std::size_t size() const override { return in_->size(); }
    const OpCounters& counters() const override { return in_->counters(); }
    const char* name() const override { return "Pass-through"; }
    void mergeFrom(PriorityQueue& other) override {
        auto* f = dynamic_cast<FaultyHeapBase*>(&other);
        if (!f) throw std::invalid_argument("type mismatch");
        in_->mergeFrom(*f->in_);
    }
    bool checkInvariants() const override { return in_->checkInvariants(); }
protected:
    std::unique_ptr<PriorityQueue> in_ = makeHeap(HeapType::Binary);
};

// A heap that is correct except that every 5th extractMin returns the second-smallest
// entry. Used to prove the benchmark rejects wrong results instead of timing them.
class FaultyHeap : public PriorityQueue {
public:
    void insert(int id, const Key& k) override { in_->insert(id, k); }
    HeapEntry extractMin() override {
        HeapEntry a = in_->extractMin();
        if (++calls_ % 5 == 0 && !in_->empty()) {
            HeapEntry b = in_->extractMin();
            in_->insert(a.id, a.key);
            return b;
        }
        return a;
    }
    const HeapEntry& peekMin() const override { return in_->peekMin(); }
    void decreaseKey(int id, const Key& k) override { in_->decreaseKey(id, k); }
    bool contains(int id) const override { return in_->contains(id); }
    bool empty() const override { return in_->empty(); }
    std::size_t size() const override { return in_->size(); }
    const OpCounters& counters() const override { return in_->counters(); }
    const char* name() const override { return "Faulty"; }
    void mergeFrom(PriorityQueue& other) override {
        auto* f = dynamic_cast<FaultyHeap*>(&other);
        if (!f) throw std::invalid_argument("type mismatch");
        in_->mergeFrom(*f->in_);
    }
    bool checkInvariants() const override { return in_->checkInvariants(); }
private:
    std::unique_ptr<PriorityQueue> in_ = makeHeap(HeapType::Binary);
    long long calls_ = 0;
};

static BenchConfig tinyConfig() {
    BenchConfig c;
    c.profile = "test";
    c.seed = 777;
    c.repeats = 3; c.warmups = 1; c.instances = 2;
    c.graphSizes = {30, 60};
    c.parcelSizes = {200};
    return c;
}

// ---- generators ------------------------------------------------------------------

static void rngAndSeeds() {
    testContext() = "rng";
    Rng a(42), b(42), c(43);
    bool same = true, differs = false;
    for (int i = 0; i < 100; ++i) { uint64_t x = a.next(); same = same && x == b.next(); differs = differs || x != c.next(); }
    CHECK(same);
    CHECK(differs);
    // Golden values: the generator must give the same numbers on every platform.
    Rng g(1);
    CHECK(g.next() == 0x910a2dec89025cc1ULL);
    CHECK(g.next() == 0xbeeb8da1658eec67ULL);

    Rng r(5);
    bool inRange = true;
    for (int i = 0; i < 2000; ++i) { long long v = r.range(3, 9); inRange = inRange && v >= 3 && v <= 9; }
    CHECK(inRange);

    CHECK(mixSeed(1, {2, 3}) == mixSeed(1, {2, 3}));
    CHECK(mixSeed(1, {2, 3}) != mixSeed(1, {3, 2}));
    CHECK(mixSeed(1, {2, 3}) != mixSeed(2, {2, 3}));
    CHECK(mixSeed(99, {1, 2, 3}) < (1ULL << 53));
}

static void graphGenerator() {
    testContext() = "graph generator";
    for (GraphDensity d : kAllDensities) {
        for (int n : {2, 3, 10, 57, 200}) {
            Graph g = generateCityGraph(n, d, 1234 + n, 50);
            CHECK(g.nodeCount() == n);
            CHECK(g.isConnected());
            CHECK(g.edgeCount() == targetEdgeCount(n, d));
            CHECK(g.edgeCount() >= static_cast<std::size_t>(n - 1));
            CHECK(g.edgeCount() <= static_cast<std::size_t>(n) * (n - 1) / 2);
            // simple graph, weights in range
            std::set<std::pair<int, int>> seen;
            bool simple = true, weightsOk = true;
            for (int u = 1; u <= n; ++u)
                for (const Edge& e : g.neighbors(u)) {
                    weightsOk = weightsOk && e.weight >= 1 && e.weight <= 50;
                    if (u < e.to) simple = simple && seen.insert({u, e.to}).second && u != e.to;
                }
            CHECK(simple);
            CHECK(weightsOk);
            CHECK(seen.size() == g.edgeCount());
        }
    }
    CHECK(generateCityGraph(1, GraphDensity::Dense, 5).edgeCount() == 0);   // single hub
    CHECK(generateCityGraph(1, GraphDensity::Sparse, 5).isConnected());

    // determinism
    CHECK(sameGraph(generateCityGraph(150, GraphDensity::Medium, 9), generateCityGraph(150, GraphDensity::Medium, 9)));
    CHECK(!sameGraph(generateCityGraph(150, GraphDensity::Medium, 9), generateCityGraph(150, GraphDensity::Medium, 10)));

    // density ordering
    std::size_t s = generateCityGraph(200, GraphDensity::Sparse, 1).edgeCount();
    std::size_t m = generateCityGraph(200, GraphDensity::Medium, 1).edgeCount();
    std::size_t d = generateCityGraph(200, GraphDensity::Dense, 1).edgeCount();
    CHECK(s < m && m < d);
    CHECK(targetEdgeCount(1000, GraphDensity::Sparse) == 2000);
    CHECK(targetEdgeCount(1000, GraphDensity::Medium) == 8000);
    CHECK(targetEdgeCount(1000, GraphDensity::Dense) == 249750);

    CHECK_THROWS(generateCityGraph(0, GraphDensity::Sparse, 1));
    CHECK_THROWS(generateCityGraph(-4, GraphDensity::Sparse, 1));
    CHECK_THROWS(generateCityGraph(10, GraphDensity::Sparse, 1, 0));

    CHECK(parseDensity("dense") == GraphDensity::Dense);
    CHECK(!parseDensity("huge").has_value());
    CHECK(std::string(toString(GraphDensity::Medium)) == "medium");
}

static void parcelGenerator() {
    testContext() = "parcel generator";
    ParcelWorkload w = generateParcelWorkload(40, 5000, 0.2, 31);
    CHECK(w.parcels.size() == 5000);
    bool idsOk = true, hubsOk = true, levelsOk = true;
    int express = 0, sameDay = 0, standard = 0;
    for (std::size_t i = 0; i < w.parcels.size(); ++i) {
        const ParcelSpec& p = w.parcels[i];
        idsOk = idsOk && p.id == static_cast<int>(i) + 1 && p.arrival == static_cast<int>(i);
        hubsOk = hubsOk && p.source >= 1 && p.source <= 40 && p.destination >= 1 && p.destination <= 40 && p.source != p.destination;
        if (p.level == ServiceLevel::Express) ++express;
        else if (p.level == ServiceLevel::SameDay) ++sameDay;
        else ++standard;
    }
    CHECK(idsOk);
    CHECK(hubsOk);
    levelsOk = express > 600 && express < 900 && sameDay > 1000 && sameDay < 1500 && standard > 2700;   // ~15/25/60%
    CHECK(levelsOk);

    std::set<int> upgraded;
    bool genuine = true;
    for (const UpgradeSpec& u : w.upgrades) {
        genuine = genuine && upgraded.insert(u.parcelId).second;
        genuine = genuine && static_cast<int>(u.newLevel) < static_cast<int>(w.parcels[u.parcelId - 1].level);
    }
    CHECK(genuine);
    CHECK(w.upgrades.size() == 1000);   // 20% of 5000, plenty of non-express parcels

    // expected order is a permutation, sorted by final key
    std::vector<int> order = expectedDispatchOrder(w);
    std::vector<int> sorted = order;
    std::sort(sorted.begin(), sorted.end());
    bool perm = sorted.size() == 5000;
    for (std::size_t i = 0; i < sorted.size(); ++i) perm = perm && sorted[i] == static_cast<int>(i) + 1;
    CHECK(perm);
    std::vector<ServiceLevel> lv(5000);
    for (int i = 0; i < 5000; ++i) lv[i] = w.parcels[i].level;
    for (const UpgradeSpec& u : w.upgrades) lv[u.parcelId - 1] = u.newLevel;
    bool ordered = true;
    for (std::size_t i = 1; i < order.size(); ++i) {
        Key a = dispatchKey(w.parcels[order[i - 1] - 1], lv[order[i - 1] - 1]);
        Key b = dispatchKey(w.parcels[order[i] - 1], lv[order[i] - 1]);
        ordered = ordered && a < b;
    }
    CHECK(ordered);

    // determinism, and edge cases
    ParcelWorkload w2 = generateParcelWorkload(40, 5000, 0.2, 31);
    CHECK(w2.upgrades.size() == w.upgrades.size() && w2.parcels[4999].source == w.parcels[4999].source);
    CHECK(generateParcelWorkload(10, 0, 0.5, 1).parcels.empty());
    CHECK(generateParcelWorkload(1, 5, 0.0, 1).upgrades.empty());           // one hub: src == dst is unavoidable but must not hang
    CHECK(generateParcelWorkload(10, 100, 1.0, 1).upgrades.size() <= 100);   // capped by non-express parcels
    CHECK_THROWS(generateParcelWorkload(0, 10, 0.1, 1));
    CHECK_THROWS(generateParcelWorkload(10, -1, 0.1, 1));
    CHECK_THROWS(generateParcelWorkload(10, 10, 1.5, 1));
}

// ---- statistics ------------------------------------------------------------------

static void statistics() {
    testContext() = "stats";
    auto near = [](double a, double b) { return std::abs(a - b) < 1e-9; };
    Stats e = computeStats({});
    CHECK(e.count == 0 && e.mean == 0 && e.stddev == 0);
    Stats one = computeStats({4.0});
    CHECK(one.count == 1 && near(one.mean, 4) && near(one.median, 4) && near(one.stddev, 0));
    Stats odd = computeStats({9, 1, 5, 3, 7});
    CHECK(odd.count == 5 && near(odd.mean, 5) && near(odd.median, 5) && near(odd.min, 1) && near(odd.max, 9));
    CHECK(near(odd.stddev, std::sqrt(10.0)));          // sample variance = 40/4 = 10
    Stats even = computeStats({10, 2, 4, 8});
    CHECK(near(even.median, 6) && near(even.mean, 6));
    Stats same = computeStats({3, 3, 3});
    CHECK(near(same.stddev, 0));
    Stats outlier = computeStats({1, 1, 1, 1, 100});
    CHECK(near(outlier.median, 1) && outlier.mean > 20);   // median resists outliers, mean does not
}

// ---- reference solutions and verifiers ---------------------------------------------

static void referenceSolutions() {
    testContext() = "reference";
    Graph g = makeSampleGraph();
    std::vector<long long> d = referenceDistances(g, 1);
    CHECK(d[1] == 0 && d[2] == 7 && d[3] == 9 && d[4] == 20 && d[5] == 20 && d[6] == 11 && d[7] == 23);
    CHECK(d[8] == INF_DIST);
    ReferenceMst m = referenceMst(g);
    CHECK(m.totalWeight == 36);
    CHECK(m.components == 2);

    // against the project's own implementations on random graphs
    bool agree = true;
    for (int seed = 1; seed <= 20; ++seed) {
        Graph r = generateCityGraph(80, kAllDensities[seed % 3], seed);
        auto bin = makeHeap(HeapType::Binary);
        auto fib = makeHeap(HeapType::Fibonacci);
        DijkstraResult dr = dijkstra(r, 1, *bin);
        PrimResult pr = prim(r, 1, *fib);
        agree = agree && verifyDijkstra(r, dr, referenceDistances(r, 1)).empty();
        agree = agree && verifyPrim(r, pr, referenceMst(r)).empty();
    }
    CHECK(agree);
}

static void verifiersCatchErrors() {
    testContext() = "verifiers";
    Graph g = generateCityGraph(60, GraphDensity::Medium, 8);
    auto h = makeHeap(HeapType::Binary);
    DijkstraResult dr = dijkstra(g, 1, *h);
    const auto ref = referenceDistances(g, 1);
    CHECK(verifyDijkstra(g, dr, ref).empty());

    DijkstraResult wrongDist = dr;
    wrongDist.dist[30] += 1;
    CHECK(!verifyDijkstra(g, wrongDist, ref).empty());
    DijkstraResult wrongPrev = dr;
    wrongPrev.prev[30] = (wrongPrev.prev[30] % 60) + 1;
    wrongPrev.prev[30] = wrongPrev.prev[30] == 30 ? 31 : wrongPrev.prev[30];
    CHECK(!verifyDijkstra(g, wrongPrev, ref).empty() || wrongPrev.prev[30] == dr.prev[30]);
    DijkstraResult shortVec = dr;
    shortVec.dist.pop_back();
    CHECK(!verifyDijkstra(g, shortVec, ref).empty());

    auto h2 = makeHeap(HeapType::Binomial);
    PrimResult pr = prim(g, 1, *h2);
    const ReferenceMst rm = referenceMst(g);
    CHECK(verifyPrim(g, pr, rm).empty());
    PrimResult heavier = pr;
    heavier.totalWeight += 1;
    CHECK(!verifyPrim(g, heavier, rm).empty());
    PrimResult missing = pr;
    missing.edges.pop_back();
    CHECK(!verifyPrim(g, missing, rm).empty());
    PrimResult fakeRoad = pr;
    fakeRoad.edges[3].weight += 1;
    CHECK(!verifyPrim(g, fakeRoad, rm).empty());
    PrimResult flag = pr;
    flag.isSpanningTree = false;
    CHECK(!verifyPrim(g, flag, rm).empty());
    // swap one tree edge for a different (non-tree) road of the same weight sum would be a different tree; a cycle:
    PrimResult cyc = pr;
    cyc.edges[5] = cyc.edges[4];
    CHECK(!verifyPrim(g, cyc, rm).empty());

    CHECK(verifyOrder({1, 2, 3}, {1, 2, 3}).empty());
    CHECK(!verifyOrder({1, 3, 2}, {1, 2, 3}).empty());
    CHECK(!verifyOrder({1, 2}, {1, 2, 3}).empty());
}

// ---- runner ----------------------------------------------------------------------

static void runnerProducesVerifiedRows() {
    testContext() = "runner";
    BenchConfig cfg = tinyConfig();
    BenchReport rep = runBenchmarks(cfg);
    CHECK(rep.ok());
    // dijkstra + prim: 3 densities x 2 sizes x 2 instances x 3 heaps = 36 each; dispatch + merge: 1 x 2 x 3 = 6 each
    CHECK(rep.rows.size() == 36 + 36 + 6 + 6);
    std::map<std::string, int> perBenchmark;
    bool statsOk = true, opsOk = true;
    for (const BenchRow& r : rep.rows) {
        ++perBenchmark[r.benchmark];
        statsOk = statsOk && r.repeats == 3 && r.timeMs.count == 3 && r.timeMs.min <= r.timeMs.median &&
                  r.timeMs.median <= r.timeMs.max && r.timeMs.min <= r.timeMs.mean && r.timeMs.mean <= r.timeMs.max &&
                  r.timeMs.min > 0 && r.timeMs.stddev >= 0;
        if (r.benchmark == "dijkstra" || r.benchmark == "prim")
            opsOk = opsOk && r.ops.inserts > 0 && r.ops.extracts == r.ops.inserts && r.edges >= r.size - 1 && !r.density.empty();
        if (r.benchmark == "dispatch")
            opsOk = opsOk && r.ops.inserts == 200 && r.ops.extracts == 200 && r.ops.decreaseKeys == r.upgrades && r.upgrades > 0;
        if (r.benchmark == "merge")
            opsOk = opsOk && r.ops.merges == 1 && r.ops.inserts == 0 && r.ops.extracts == 0;
    }
    CHECK(statsOk);
    CHECK(opsOk);
    CHECK(perBenchmark["dijkstra"] == 36 && perBenchmark["prim"] == 36 && perBenchmark["dispatch"] == 6 && perBenchmark["merge"] == 6);

    // Same input -> every heap does the same inserts/extracts/decrease-keys (only comparisons may differ).
    bool sameWork = true;
    for (const BenchRow& a : rep.rows)
        for (const BenchRow& b : rep.rows)
            if (a.benchmark == b.benchmark && a.density == b.density && a.size == b.size && a.instance == b.instance && a.benchmark != "merge")
                sameWork = sameWork && a.ops.inserts == b.ops.inserts && a.ops.extracts == b.ops.extracts &&
                           a.ops.decreaseKeys == b.ops.decreaseKeys && a.seed == b.seed;
    CHECK(sameWork);

    CHECK(rep.meta.seed == 777 && rep.meta.repeats == 3 && rep.meta.warmups == 1 && rep.meta.instances == 2);
    CHECK(!rep.meta.timestampUtc.empty() && !rep.meta.compiler.empty());
}

static void runnerIsReproducible() {
    testContext() = "reproducibility";
    BenchConfig cfg = tinyConfig();
    BenchReport a = runBenchmarks(cfg), b = runBenchmarks(cfg);
    CHECK(a.rows.size() == b.rows.size());
    bool identical = a.rows.size() == b.rows.size();
    for (std::size_t i = 0; identical && i < a.rows.size(); ++i) {
        const BenchRow &x = a.rows[i], &y = b.rows[i];
        identical = x.seed == y.seed && x.edges == y.edges && x.upgrades == y.upgrades && x.heap == y.heap &&
                    x.ops.comparisons == y.ops.comparisons && x.ops.decreaseKeys == y.ops.decreaseKeys && x.ops.inserts == y.ops.inserts;
    }
    CHECK(identical);   // timings differ run to run; inputs and operation counts must not

    cfg.seed = 778;
    BenchReport c = runBenchmarks(cfg);
    bool differs = false;
    for (std::size_t i = 0; i < std::min(a.rows.size(), c.rows.size()); ++i)
        differs = differs || a.rows[i].seed != c.rows[i].seed || a.rows[i].ops.comparisons != c.rows[i].ops.comparisons;
    CHECK(differs);
}

static void runnerSelectionAndValidation() {
    testContext() = "runner options";
    BenchConfig cfg = tinyConfig();
    cfg.runPrim = cfg.runDispatch = cfg.runMerge = false;
    cfg.heaps = {HeapType::Fibonacci};
    cfg.densities = {GraphDensity::Sparse};
    cfg.instances = 1;
    BenchReport rep = runBenchmarks(cfg);
    CHECK(rep.rows.size() == 2);   // two sizes, one heap
    bool onlyFib = true;
    for (const BenchRow& r : rep.rows) onlyFib = onlyFib && r.benchmark == "dijkstra" && r.heap == "Fibonacci Heap" && r.density == "sparse";
    CHECK(onlyFib);

    // dense graphs above maxDenseNodes are skipped
    BenchConfig skip = tinyConfig();
    skip.runPrim = skip.runDispatch = skip.runMerge = false;
    skip.densities = {GraphDensity::Dense};
    skip.maxDenseNodes = 40;
    skip.instances = 1;
    BenchReport s = runBenchmarks(skip);
    CHECK(s.rows.size() == 3);     // only the 30-hub graph x 3 heaps
    for (const BenchRow& r : s.rows) CHECK(r.size == 30);

    BenchConfig bad = tinyConfig();
    bad.repeats = 0;           CHECK_THROWS(runBenchmarks(bad));
    bad = tinyConfig(); bad.instances = 0;     CHECK_THROWS(runBenchmarks(bad));
    bad = tinyConfig(); bad.warmups = -1;      CHECK_THROWS(runBenchmarks(bad));
    bad = tinyConfig(); bad.heaps.clear();     CHECK_THROWS(runBenchmarks(bad));
    bad = tinyConfig(); bad.graphSizes = {1};  CHECK_THROWS(runBenchmarks(bad));
    bad = tinyConfig(); bad.upgradeFraction = 2; CHECK_THROWS(runBenchmarks(bad));
    CHECK_THROWS(profileConfig("nonsense"));
    CHECK(profileConfig("quick").repeats == 3);
    CHECK(profileConfig("standard").graphSizes.back() == 5000);
}

static void wrongResultsAreRejected() {
    testContext() = "fault injection";
    BenchConfig cfg = tinyConfig();
    cfg.instances = 1;
    cfg.provider = [](HeapType t) -> std::unique_ptr<PriorityQueue> {
        if (t == HeapType::Fibonacci) return std::make_unique<FaultyHeap>();   // pretends to be the Fibonacci heap
        return makeHeap(t);
    };
    BenchReport rep = runBenchmarks(cfg);
    CHECK(!rep.ok());
    bool dispatchFailed = false, mergeFailed = false, primFailed = false, onlyFib = true;
    for (const BenchFailure& f : rep.failures) {
        onlyFib = onlyFib && f.heap == "Fibonacci Heap" && !f.message.empty();
        dispatchFailed = dispatchFailed || f.benchmark == "dispatch";
        mergeFailed = mergeFailed || f.benchmark == "merge";
        primFailed = primFailed || f.benchmark == "prim";
    }
    CHECK(onlyFib);              // the honest heaps are never blamed
    CHECK(dispatchFailed);
    CHECK(mergeFailed);
    CHECK(primFailed);
    bool noBadRows = true, goodRows = false;
    for (const BenchRow& r : rep.rows) {
        noBadRows = noBadRows && !(r.heap == "Fibonacci Heap" && (r.benchmark == "dispatch" || r.benchmark == "merge"));
        goodRows = goodRows || r.heap == "Binary Heap";
    }
    CHECK(noBadRows);            // timings of a wrong implementation are never exported
    CHECK(goodRows);             // but the correct heaps still produce results

    std::ostringstream summary;
    printSummary(rep, summary);
    CHECK(summary.str().find("VERIFICATION FAILURES") != std::string::npos);
    std::string csv = toCsv(rep);
    CHECK(csv.find("Faulty") == std::string::npos);
}

static void heapOrderRotates() {
    testContext() = "heap rotation";
    BenchConfig cfg;
    cfg.repeats = 3; cfg.warmups = 0; cfg.instances = 1;
    cfg.graphSizes = {20};
    cfg.densities = {GraphDensity::Sparse};
    cfg.runPrim = cfg.runDispatch = cfg.runMerge = false;
    std::vector<HeapType> created;
    cfg.provider = [&created](HeapType t) { created.push_back(t); return makeHeap(t); };
    BenchReport rep = runBenchmarks(cfg);
    CHECK(rep.ok());
    // one heap per run: round 0 = B,Bi,F  round 1 = Bi,F,B  round 2 = F,B,Bi
    const HeapType B = HeapType::Binary, Bi = HeapType::Binomial, F = HeapType::Fibonacci;
    std::vector<HeapType> expected{B, Bi, F, Bi, F, B, F, B, Bi};
    CHECK(created == expected);
}

// Reports a different comparison count on every run, as a non-reproducible heap would.
class DriftHeap : public FaultyHeapBase {
public:
    const OpCounters& counters() const override {
        shown_ = FaultyHeapBase::counters();
        shown_.comparisons += ++runs_;
        return shown_;
    }
private:
    mutable OpCounters shown_;
    static inline long long runs_ = 0;
};

static void nonReproducibleCountsAreRejected() {
    testContext() = "reproducibility check";
    BenchConfig cfg = tinyConfig();
    cfg.instances = 1;
    cfg.runPrim = cfg.runDispatch = cfg.runMerge = false;
    cfg.provider = [](HeapType t) -> std::unique_ptr<PriorityQueue> {
        if (t == HeapType::Binomial) return std::make_unique<DriftHeap>();
        return makeHeap(t);
    };
    BenchReport rep = runBenchmarks(cfg);
    bool flagged = false;
    for (const BenchFailure& f : rep.failures) flagged = flagged || f.message.find("not reproducible") != std::string::npos;
    CHECK(flagged);
    CHECK(!rep.ok());
}

static void timerExcludesSetup() {
    testContext() = "timer scope";
    BenchConfig cfg;
    cfg.repeats = 3; cfg.warmups = 1; cfg.instances = 1;
    cfg.graphSizes = {30};
    cfg.densities = {GraphDensity::Sparse};
    cfg.runPrim = cfg.runDispatch = cfg.runMerge = false;
    cfg.provider = [](HeapType t) {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));    // slow heap construction
        return makeHeap(t);
    };
    auto t0 = std::chrono::steady_clock::now();
    BenchReport rep = runBenchmarks(cfg);
    double wallMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    CHECK(rep.ok() && rep.rows.size() == 3);
    CHECK(wallMs > 3 * 4 * 30 * 0.9);          // the sleeps really happened (3 heaps x 4 runs x 30 ms)
    bool small = true;
    for (const BenchRow& r : rep.rows) small = small && r.timeMs.max < 15.0;
    CHECK(small);                               // ...yet none of that 30 ms shows up in the measured time
}

// ---- export ----------------------------------------------------------------------

static std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (quoted) {
            if (c == '"' && i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; ++i; }
            else if (c == '"') quoted = false;
            else cur += c;
        } else if (c == '"') quoted = true;
        else if (c == ',') { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
    return out;
}

static void exportFormats() {
    testContext() = "export";
    CHECK(csvEscape("plain") == "plain");
    CHECK(csvEscape("a,b") == "\"a,b\"");
    CHECK(csvEscape("say \"hi\"") == "\"say \"\"hi\"\"\"");
    CHECK(jsonEscape("a\"b\\c\n\t") == "a\\\"b\\\\c\\n\\t");
    CHECK(jsonEscape(std::string(1, '\x01')) == "\\u0001");

    BenchConfig cfg = tinyConfig();
    cfg.instances = 1;
    BenchReport rep = runBenchmarks(cfg);
    BenchFailure f;                         // a failure message that needs escaping
    f.benchmark = "prim"; f.heap = "Test, Heap"; f.size = 5; f.message = "line1\nsaid \"no\"";
    rep.failures.push_back(f);

    std::string csv = toCsv(rep);
    std::istringstream in(csv);
    std::string line;
    std::getline(in, line);
    std::vector<std::string> header = splitCsvLine(line);
    CHECK(header.size() == 20 && header[0] == "benchmark" && header[10] == "median_ms" && header.back() == "verified");
    int lines = 0;
    bool widthOk = true, numbersOk = true;
    while (std::getline(in, line)) {
        ++lines;
        std::vector<std::string> cols = splitCsvLine(line);
        widthOk = widthOk && cols.size() == header.size();
        if (cols.size() == header.size()) {
            double median = std::stod(cols[10]);
            numbersOk = numbersOk && median > 0 && cols.back() == "true" && std::stoll(cols[3]) == rep.rows[lines - 1].size;
        }
    }
    CHECK(lines == static_cast<int>(rep.rows.size()));
    CHECK(widthOk);
    CHECK(numbersOk);

    std::string json = toJson(rep);
    int braces = 0, brackets = 0;
    bool inString = false, balanced = true;
    for (std::size_t i = 0; i < json.size(); ++i) {
        char c = json[i];
        if (inString) { if (c == '\\') ++i; else if (c == '"') inString = false; continue; }
        if (c == '"') inString = true;
        else if (c == '{') ++braces;
        else if (c == '}') --braces;
        else if (c == '[') ++brackets;
        else if (c == ']') --brackets;
        balanced = balanced && braces >= 0 && brackets >= 0;
    }
    CHECK(balanced && braces == 0 && brackets == 0 && !inString);
    CHECK(json.find("\"meta\"") != std::string::npos && json.find("\"rows\"") != std::string::npos);
    CHECK(json.find("line1\\nsaid \\\"no\\\"") != std::string::npos);
    CHECK(json.find("\"density\": null") != std::string::npos);   // dispatch/merge rows have no density
    std::size_t rowCount = 0, pos = 0;
    while ((pos = json.find("\"verified\": true", pos)) != std::string::npos) { ++rowCount; ++pos; }
    CHECK(rowCount == rep.rows.size());

    // empty report still produces valid, parseable output
    BenchReport empty;
    std::string ej = toJson(empty);
    CHECK(ej.find("\"rows\": []") != std::string::npos && ej.find("\"failures\": []") != std::string::npos);
    CHECK(toCsv(empty).find('\n') == toCsv(empty).size() - 1);   // header only

    // file writing, including creating missing folders
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "parcelhub_bench_test" / "nested";
    fs::remove_all(fs::temp_directory_path() / "parcelhub_bench_test");
    std::string err;
    CHECK(writeTextFile((dir / "out.csv").string(), csv, err));
    std::ifstream back((dir / "out.csv").string(), std::ios::binary);
    std::stringstream ss;
    ss << back.rdbuf();
    CHECK(ss.str() == csv);
    CHECK(!writeTextFile((dir / "out.csv" / "child.txt").string(), "x", err));   // parent is a file
    CHECK(!err.empty());
    fs::remove_all(fs::temp_directory_path() / "parcelhub_bench_test");
}

int main() {
    rngAndSeeds();
    graphGenerator();
    parcelGenerator();
    statistics();
    referenceSolutions();
    verifiersCatchErrors();
    runnerProducesVerifiedRows();
    runnerIsReproducible();
    runnerSelectionAndValidation();
    wrongResultsAreRejected();
    heapOrderRotates();
    nonReproducibleCountsAreRejected();
    timerExcludesSetup();
    exportFormats();
    return finishTests("benchmarkTests");
}

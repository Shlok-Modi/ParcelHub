// Feeds identical workloads to all three heaps and checks they agree with a
// reference model and with each other (heaps, Dijkstra and the dispatch engine).
#include <algorithm>
#include <map>
#include <memory>
#include <random>
#include <vector>
#include "Dijkstra.h"
#include "DispatchEngine.h"
#include "Graph.h"
#include "HeapFactory.h"
#include "TestUtil.h"

static void randomOperationsAgree(unsigned seed) {
    testContext() = "cross-heap random ops";
    std::mt19937 rng(seed);
    std::vector<std::unique_ptr<PriorityQueue>> heaps;
    for (HeapType t : kAllHeapTypes) heaps.push_back(makeHeap(t));

    std::map<Key, int> byKey;       // reference model: ordered by key
    std::map<int, Key> live;
    int nextId = 0;
    bool allAgree = true;

    auto addToModel = [&](int id, const Key& k) { byKey[k] = id; live[id] = k; };

    for (int op = 0; op < 15000; ++op) {
        int r = rng() % 100;
        if (r < 40 || live.empty()) {
            Key k{(long long)(rng() % 200), (long long)(rng() % 50), nextId};
            for (auto& h : heaps) h->insert(nextId, k);
            addToModel(nextId, k); ++nextId;
        } else if (r < 65) {
            auto it = byKey.begin();
            int expectId = it->second; Key expectKey = it->first;
            byKey.erase(it); live.erase(expectId);
            for (auto& h : heaps) {
                HeapEntry e = h->extractMin();
                allAgree = allAgree && e.id == expectId && e.key == expectKey;
            }
        } else if (r < 85) {
            auto it = live.begin();
            std::advance(it, rng() % live.size());
            int id = it->first; Key old = it->second;
            Key nk{old.primary - (long long)(rng() % 30), old.secondary - (long long)(rng() % 3), id};
            for (auto& h : heaps) h->decreaseKey(id, nk);
            byKey.erase(old); addToModel(id, nk);
        } else if (r < 92) {
            std::vector<std::unique_ptr<PriorityQueue>> side;
            for (HeapType t : kAllHeapTypes) side.push_back(makeHeap(t));
            int k = 1 + rng() % 20;
            for (int j = 0; j < k; ++j) {
                Key key{(long long)(rng() % 200), (long long)(rng() % 50), nextId};
                for (auto& s : side) s->insert(nextId, key);
                addToModel(nextId, key); ++nextId;
            }
            for (std::size_t i = 0; i < heaps.size(); ++i) {
                heaps[i]->mergeFrom(*side[i]);
                allAgree = allAgree && side[i]->empty();
            }
        } else {
            for (auto& h : heaps) allAgree = allAgree && h->peekMin().key == byKey.begin()->first;
        }
        if (op % 500 == 0) {
            for (auto& h : heaps) { CHECK(h->checkInvariants()); allAgree = allAgree && h->size() == live.size(); }
        }
    }
    CHECK(allAgree);
    // drain everything and compare against the model
    bool drained = true;
    while (!byKey.empty()) {
        auto it = byKey.begin();
        for (auto& h : heaps) drained = drained && h->extractMin().id == it->second;
        byKey.erase(it);
    }
    for (auto& h : heaps) drained = drained && h->empty();
    CHECK(drained);
}

static Graph randomGraph(std::mt19937& rng, int n, int extraEdges) {
    Graph g(n);
    for (int v = 2; v <= n; ++v)
        if (rng() % 10 != 0) g.addEdge(v, 1 + rng() % (v - 1), 1 + rng() % 50);   // mostly connected
    for (int i = 0; i < extraEdges; ++i) {
        int u = 1 + rng() % n, v = 1 + rng() % n;
        if (u != v) g.addEdge(u, v, rng() % 60);
    }
    return g;
}

static void dijkstraAgrees() {
    testContext() = "cross-heap Dijkstra";
    std::mt19937 rng(31337);
    for (int trial = 0; trial < 30; ++trial) {
        int n = 10 + rng() % 90;
        Graph g = randomGraph(rng, n, n * 2);
        int s = 1 + rng() % n;
        auto base = makeHeap(HeapType::Binary);
        DijkstraResult ref = dijkstra(g, s, *base);
        for (HeapType t : kAllHeapTypes) {
            auto h = makeHeap(t);
            DijkstraResult got = dijkstra(g, s, *h);
            CHECK(got.dist == ref.dist);
            CHECK(got.prev == ref.prev);   // keys are unique, so even the paths are identical
        }
    }
}

static void dispatchAgrees() {
    testContext() = "cross-heap dispatch";
    std::mt19937 rng(2025);
    Graph g = randomGraph(rng, 25, 40);

    struct Spec { int id, src, dst, level, arrival; };
    std::vector<Spec> specs;
    for (int i = 1; i <= 300; ++i)
        specs.push_back({i, (int)(1 + rng() % 25), (int)(1 + rng() % 25), (int)(1 + rng() % 3), (int)(rng() % 40)});
    std::vector<std::pair<int, int>> upgrades;   // (id, new level)
    for (int i = 1; i <= 300; i += 4) upgrades.push_back({i, 1 + (int)(rng() % 2)});

    // independent expectation: final level per parcel -> sorted order
    std::map<int, int> finalLevel;
    for (const auto& s : specs) finalLevel[s.id] = s.level;
    for (auto [id, lvl] : upgrades) if (lvl < finalLevel[id]) finalLevel[id] = lvl;
    std::vector<Spec> sorted = specs;
    std::sort(sorted.begin(), sorted.end(), [&](const Spec& a, const Spec& b) {
        return std::tie(finalLevel[a.id], a.arrival, a.id) < std::tie(finalLevel[b.id], b.arrival, b.id); });
    std::vector<int> expected;
    for (const auto& s : sorted) expected.push_back(s.id);

    struct Out { int id; bool reachable; long long cost; std::vector<int> path; };
    auto key = [](const Out& o) { return std::tie(o.id, o.reachable, o.cost, o.path); };
    std::vector<std::vector<Out>> runs;

    for (HeapType t : kAllHeapTypes) {
        testContext() = toString(t);
        DispatchEngine a(g, t), b(g, t);
        for (const auto& s : specs)
            (s.id <= 150 ? a : b).addParcel(makeParcel(s.id, s.src, s.dst, static_cast<ServiceLevel>(s.level), 0, s.arrival));
        for (auto [id, lvl] : upgrades) {
            DispatchEngine& e = id <= 150 ? a : b;
            if (lvl < static_cast<int>(e.find(id)->level)) e.upgradePriority(id, static_cast<ServiceLevel>(lvl));
        }
        CHECK(a.pendingCount() == 150 && b.pendingCount() == 150);
        a.mergeFrom(b);                                        // combine the two hub queues
        CHECK(a.pendingCount() == 300 && a.totalCount() == 300 && b.totalCount() == 0 && b.pendingCount() == 0);

        std::vector<Out> out;
        while (auto r = a.dispatchNext()) out.push_back({r->parcel.id, r->reachable, r->cost, r->path});
        CHECK(a.dispatchedCount() == 300 && a.pendingCount() == 0);
        std::vector<int> ids;
        for (const auto& o : out) ids.push_back(o.id);
        CHECK(ids == expected);                                // matches the independent sort
        runs.push_back(std::move(out));
    }
    testContext() = "cross-heap dispatch";
    for (std::size_t i = 1; i < runs.size(); ++i) {
        bool same = runs[i].size() == runs[0].size();
        for (std::size_t j = 0; same && j < runs[0].size(); ++j) same = key(runs[i][j]) == key(runs[0][j]);
        CHECK(same);                                           // same order, routes and costs for every heap
    }

    // Engine-level merge validation
    DispatchEngine x(g, HeapType::Binomial), y(g, HeapType::Binomial), z(g, HeapType::Fibonacci);
    x.addParcel(makeParcel(1, 1, 2, ServiceLevel::Express, 0, 0));
    y.addParcel(makeParcel(1, 1, 2, ServiceLevel::Express, 0, 1));
    z.addParcel(makeParcel(2, 1, 2, ServiceLevel::Express, 0, 2));
    CHECK_THROWS(x.mergeFrom(y));       // same parcel id in both hubs
    CHECK_THROWS(x.mergeFrom(z));       // different heap types
    CHECK_THROWS(x.mergeFrom(x));
    CHECK(x.totalCount() == 1 && y.totalCount() == 1 && z.totalCount() == 1);
    Graph other(3);
    DispatchEngine w(other, HeapType::Binomial);
    CHECK_THROWS(x.mergeFrom(w));       // different graphs
}

int main() {
    for (unsigned seed : {1u, 2u, 3u, 4u, 5u}) randomOperationsAgree(seed);
    dijkstraAgrees();
    dispatchAgrees();
    return finishTests("crossHeapTests");
}

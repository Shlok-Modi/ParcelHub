#include <algorithm>
#include <numeric>
#include <random>
#include <set>
#include <vector>
#include "Graph.h"
#include "HeapFactory.h"
#include "Prim.h"
#include "TestUtil.h"

struct DSU {
    std::vector<int> p;
    explicit DSU(int n) : p(n + 1) { std::iota(p.begin(), p.end(), 0); }
    int find(int x) { while (p[x] != x) { p[x] = p[p[x]]; x = p[x]; } return x; }
    bool unite(int a, int b) { a = find(a); b = find(b); if (a == b) return false; p[a] = b; return true; }
};

// Independent reference: Kruskal with union-find (no heap involved).
static long long kruskalWeight(const Graph& g) {
    struct E { int u, v; long long w; };
    std::vector<E> edges;
    for (int u = 1; u <= g.nodeCount(); ++u)
        for (const Edge& e : g.neighbors(u))
            if (u < e.to) edges.push_back({u, e.to, e.weight});   // each (parallel) edge once
    std::sort(edges.begin(), edges.end(), [](const E& a, const E& b) { return a.w < b.w; });
    DSU d(g.nodeCount());
    long long total = 0;
    for (const E& e : edges) if (d.unite(e.u, e.v)) total += e.w;
    return total;
}

// Everything a correct minimum spanning forest must satisfy.
static void checkForest(const Graph& g, const PrimResult& r) {
    const int n = g.nodeCount();
    DSU d(n);
    long long sum = 0;
    for (const MstEdge& e : r.edges) {
        bool exists = false;
        for (const Edge& ge : g.neighbors(e.u)) exists = exists || (ge.to == e.v && ge.weight == e.weight);
        CHECK(exists);                                  // every tree edge is a real road
        CHECK(d.unite(e.u, e.v));                       // acyclic: never joins two already-connected hubs
        sum += e.weight;
    }
    CHECK(sum == r.totalWeight);
    CHECK(r.totalWeight == kruskalWeight(g));           // minimum weight, per the independent reference
    CHECK(r.components == g.componentCount());
    CHECK(r.isSpanningTree == (g.componentCount() == 1));
    CHECK((int)r.edges.size() == n - r.components);     // a forest of c trees has n - c edges
    // the forest connects exactly what the graph connects
    for (int u = 1; u <= n; ++u)
        for (const Edge& e : g.neighbors(u)) {
            CHECK(d.find(u) == d.find(e.to));
            CHECK(r.componentOf[u] == r.componentOf[e.to]);
        }
    std::set<int> comps;
    for (int u = 1; u <= n; ++u) { CHECK(r.componentOf[u] >= 1 && r.componentOf[u] <= r.components); comps.insert(r.componentOf[u]); }
    CHECK((int)comps.size() == r.components);
    // every hub is inserted and extracted exactly once
    CHECK(r.heapOps.inserts == n && r.heapOps.extracts == n);
}

int main() {
    // ---- hand-checked sample graph (H8 isolated) ----
    for (HeapType t : kAllHeapTypes) {
        testContext() = toString(t);
        Graph g = makeSampleGraph();
        auto h = makeHeap(t);
        PrimResult r = prim(g, 1, *h);
        CHECK(r.totalWeight == 36);
        CHECK(r.components == 2 && !r.isSpanningTree);
        CHECK(r.edges.size() == 6);
        CHECK((r.edges == std::vector<MstEdge>{{1, 2, 7}, {1, 3, 9}, {3, 6, 2}, {6, 5, 9}, {5, 7, 3}, {5, 4, 6}}));
        CHECK(r.componentOf[8] == 2 && r.componentOf[1] == 1 && r.componentOf[7] == 1);
        checkForest(g, r);
        CHECK(h->empty());
    }

    testContext() = "edge cases";
    // connected graph -> a real spanning tree
    {
        Graph g(4);
        g.addEdge(1, 2, 3); g.addEdge(2, 3, 1); g.addEdge(3, 4, 2); g.addEdge(1, 4, 10); g.addEdge(1, 3, 5);
        auto h = makeHeap(HeapType::Fibonacci);
        PrimResult r = prim(g, 2, *h);
        CHECK(r.isSpanningTree && r.components == 1 && r.totalWeight == 6 && r.edges.size() == 3);
        checkForest(g, r);
    }
    // single hub, no edges
    { Graph g(1); auto h = makeHeap(HeapType::Binary); PrimResult r = prim(g, 1, *h);
      CHECK(r.isSpanningTree && r.edges.empty() && r.totalWeight == 0 && r.components == 1); }
    // no edges at all: n singleton components, never reported as a tree
    { Graph g(5); auto h = makeHeap(HeapType::Binomial); PrimResult r = prim(g, 3, *h);
      CHECK(!r.isSpanningTree && r.components == 5 && r.edges.empty() && r.totalWeight == 0);
      checkForest(g, r); }
    // zero-weight edges and parallel edges (cheapest wins)
    {
        Graph g(3);
        g.addEdge(1, 2, 0); g.addEdge(1, 2, 7); g.addEdge(2, 3, 4); g.addEdge(2, 3, 0); g.addEdge(1, 3, 2);
        for (HeapType t : kAllHeapTypes) { auto h = makeHeap(t); PrimResult r = prim(g, 1, *h);
            CHECK(r.totalWeight == 0 && r.edges.size() == 2); checkForest(g, r); }
    }
    // all weights equal: any spanning tree is minimum; result must still be valid and identical per heap
    {
        Graph g(6);
        for (int u = 1; u <= 6; ++u) for (int v = u + 1; v <= 6; ++v) g.addEdge(u, v, 5);
        std::vector<MstEdge> first;
        for (HeapType t : kAllHeapTypes) { auto h = makeHeap(t); PrimResult r = prim(g, 1, *h);
            CHECK(r.totalWeight == 25); checkForest(g, r);
            if (first.empty()) first = r.edges; else CHECK(r.edges == first); }
    }
    // input validation
    {
        Graph g = makeSampleGraph();
        auto h = makeHeap(HeapType::Binary);
        CHECK_THROWS(prim(g, 0, *h)); CHECK_THROWS(prim(g, 9, *h));
        h->insert(1, {0, 0, 1});
        CHECK_THROWS(prim(g, 1, *h));
    }

    // ---- randomised: compare with Kruskal; every start hub; identical on all heaps ----
    testContext() = "random graphs";
    std::mt19937 rng(8675309);
    for (int trial = 0; trial < 120; ++trial) {
        int n = 1 + rng() % 60;
        Graph g(n);
        int m = (int)(rng() % (3 * n + 1));
        int maxW = (trial % 3 == 0) ? 3 : 100;           // small range -> many ties and zero weights
        for (int i = 0; i < m; ++i) {
            int u = 1 + rng() % n, v = 1 + rng() % n;
            if (u != v) g.addEdge(u, v, (long long)(rng() % (maxW + 1)));
        }
        int start = 1 + rng() % n;
        std::vector<MstEdge> baseEdges;
        long long baseTotal = -1;
        for (HeapType t : kAllHeapTypes) {
            auto h = makeHeap(t);
            PrimResult r = prim(g, start, *h);
            checkForest(g, r);
            if (baseTotal < 0) { baseEdges = r.edges; baseTotal = r.totalWeight; }
            CHECK(r.edges == baseEdges && r.totalWeight == baseTotal);   // same edges, same order, on every heap
        }
        // total weight does not depend on the start hub
        auto h2 = makeHeap(HeapType::Fibonacci);
        CHECK(prim(g, 1 + rng() % n, *h2).totalWeight == baseTotal);
    }
    return finishTests("primTests");
}

#include <memory>
#include <random>
#include <vector>
#include "BinaryHeap.h"
#include "Dijkstra.h"
#include "Graph.h"
#include "TestUtil.h"

// Independent O(V^2) reference implementation (no heap) for cross-checking.
static std::vector<long long> referenceDijkstra(const Graph& g, int s) {
    int n = g.nodeCount();
    std::vector<long long> d(n + 1, INF_DIST);
    std::vector<char> done(n + 1, 0);
    d[s] = 0;
    for (int it = 0; it < n; ++it) {
        int u = -1;
        for (int v = 1; v <= n; ++v) if (!done[v] && (u == -1 || d[v] < d[u])) u = v;
        if (u == -1 || d[u] >= INF_DIST) break;
        done[u] = 1;
        for (const Edge& e : g.neighbors(u)) d[e.to] = std::min(d[e.to], d[u] + e.weight);
    }
    return d;
}

int main() {
    // Graph validation
    { Graph g(3); CHECK_THROWS(g.addEdge(0, 1, 1)); CHECK_THROWS(g.addEdge(1, 4, 1));
      CHECK_THROWS(g.addEdge(1, 2, -1)); CHECK_THROWS(g.addEdge(2, 2, 1)); CHECK_THROWS(Graph(0));
      g.addEdge(1, 2, 0); CHECK(g.edgeCount() == 1); CHECK(!g.isConnected()); g.addEdge(2, 3, 4); CHECK(g.isConnected()); }

    // Hand-checked distances on the sample graph from H1
    Graph g = makeSampleGraph();
    CHECK(g.edgeCount() == 10);
    CHECK(!g.isConnected());
    BinaryHeap h;
    DijkstraResult r = dijkstra(g, 1, h);
    CHECK(r.dist[1] == 0);  CHECK(r.dist[2] == 7);  CHECK(r.dist[3] == 9);  CHECK(r.dist[4] == 20);
    CHECK(r.dist[5] == 20); CHECK(r.dist[6] == 11); CHECK(r.dist[7] == 23); CHECK(r.dist[8] == INF_DIST);

    // Path reconstruction
    CHECK((reconstructPath(r, 5) == std::vector<int>{1, 3, 6, 5}));
    CHECK((reconstructPath(r, 7) == std::vector<int>{1, 3, 6, 5, 7}));
    CHECK((reconstructPath(r, 1) == std::vector<int>{1}));   // source == destination
    CHECK(reconstructPath(r, 8).empty());                    // unreachable
    CHECK(reconstructPath(r, 99).empty());                   // out of range

    // Input validation
    { BinaryHeap p; CHECK_THROWS(dijkstra(g, 0, p)); CHECK_THROWS(dijkstra(g, 9, p));
      p.insert(1, {0, 0, 1}); CHECK_THROWS(dijkstra(g, 1, p)); }

    // Randomised cross-check against the reference (fixed seed, many graphs)
    std::mt19937 rng(2024);
    for (int trial = 0; trial < 50; ++trial) {
        int n = 5 + rng() % 60;
        Graph rg(n);
        int m = n + rng() % (3 * n);
        for (int i = 0; i < m; ++i) {
            int u = 1 + rng() % n, v = 1 + rng() % n;
            if (u != v) rg.addEdge(u, v, rng() % 100);   // includes zero weights
        }
        int s = 1 + rng() % n;
        BinaryHeap hp;
        DijkstraResult got = dijkstra(rg, s, hp);
        CHECK(got.dist == referenceDijkstra(rg, s));
        // each reconstructed path must sum to its reported distance
        for (int v = 1; v <= n; ++v) {
            auto path = reconstructPath(got, v);
            if (got.dist[v] >= INF_DIST) { CHECK(path.empty()); continue; }
            long long sum = 0;
            for (std::size_t i = 1; i < path.size(); ++i) {
                long long best = INF_DIST;
                for (const Edge& e : rg.neighbors(path[i - 1])) if (e.to == path[i]) best = std::min(best, e.weight);
                sum += best;
            }
            CHECK(sum == got.dist[v]);
        }
    }
    // Edge cases: single hub, parallel edges, zero-weight chain, unreachable-only graph
    {
        Graph one(1);
        BinaryHeap p1;
        DijkstraResult r1 = dijkstra(one, 1, p1);
        CHECK(r1.dist[1] == 0 && (reconstructPath(r1, 1) == std::vector<int>{1}));

        Graph par(2);
        par.addEdge(1, 2, 5); par.addEdge(1, 2, 3); par.addEdge(1, 2, 9);
        BinaryHeap p2;
        CHECK(dijkstra(par, 1, p2).dist[2] == 3);                 // cheapest parallel edge wins

        Graph zero(4);
        zero.addEdge(1, 2, 0); zero.addEdge(2, 3, 0); zero.addEdge(3, 4, 0);
        BinaryHeap p3;
        DijkstraResult r3 = dijkstra(zero, 1, p3);
        CHECK(r3.dist[4] == 0 && (reconstructPath(r3, 4) == std::vector<int>{1, 2, 3, 4}));

        Graph none(3);                                            // no edges at all
        BinaryHeap p4;
        DijkstraResult r4 = dijkstra(none, 2, p4);
        CHECK(r4.dist[2] == 0 && r4.dist[1] >= INF_DIST && r4.dist[3] >= INF_DIST);
        CHECK(reconstructPath(r4, 1).empty());

        // Equal-cost routes: the path chosen must be the same every run and sum to the right cost
        Graph tie(4);
        tie.addEdge(1, 2, 1); tie.addEdge(1, 3, 1); tie.addEdge(2, 4, 1); tie.addEdge(3, 4, 1);
        BinaryHeap p5, p6;
        CHECK(dijkstra(tie, 1, p5).prev == dijkstra(tie, 1, p6).prev);
        CHECK(dijkstra(tie, 1, *std::make_unique<BinaryHeap>()).dist[4] == 2);
    }
    CHECK(makeSampleGraph().componentCount() == 2);
    return finishTests("routingTests");
}

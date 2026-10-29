#include "Prim.h"
#include "Dijkstra.h"   // INF_DIST
#include <stdexcept>

PrimResult prim(const Graph& g, int start, PriorityQueue& pq) {
    if (!g.isValidNode(start)) throw std::invalid_argument("prim: invalid start hub");
    if (!pq.empty()) throw std::invalid_argument("prim: priority queue must start empty");

    const int n = g.nodeCount();
    PrimResult r;
    r.componentOf.assign(n + 1, 0);
    std::vector<char> inTree(n + 1, 0);
    std::vector<long long> best(n + 1, INF_DIST);   // cheapest known edge into the tree
    std::vector<int> parent(n + 1, 0);

    auto growFrom = [&](int s) {
        ++r.components;
        best[s] = 0;
        pq.insert(s, {0, 0, s});
        while (!pq.empty()) {
            int u = pq.extractMin().id;
            inTree[u] = 1;
            r.componentOf[u] = r.components;
            if (parent[u] != 0) {
                r.edges.push_back({parent[u], u, best[u]});
                r.totalWeight += best[u];
            }
            for (const Edge& e : g.neighbors(u)) {
                int v = e.to;
                if (inTree[v] || e.weight >= best[v]) continue;
                best[v] = e.weight;
                parent[v] = u;
                Key k{e.weight, 0, v};
                if (pq.contains(v)) pq.decreaseKey(v, k);
                else pq.insert(v, k);
            }
        }
    };

    growFrom(start);
    for (int s = 1; s <= n; ++s)
        if (!inTree[s]) growFrom(s);

    r.isSpanningTree = (r.components == 1);
    r.heapOps = pq.counters();
    return r;
}

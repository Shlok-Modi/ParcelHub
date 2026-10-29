#include "Dijkstra.h"
#include <algorithm>
#include <stdexcept>

DijkstraResult dijkstra(const Graph& g, int source, PriorityQueue& pq) {
    if (!g.isValidNode(source)) throw std::invalid_argument("dijkstra: invalid source hub");
    if (!pq.empty()) throw std::invalid_argument("dijkstra: priority queue must start empty");

    DijkstraResult r;
    r.source = source;
    r.dist.assign(g.nodeCount() + 1, INF_DIST);
    r.prev.assign(g.nodeCount() + 1, -1);

    r.dist[source] = 0;
    pq.insert(source, {0, 0, source});
    while (!pq.empty()) {
        int u = pq.extractMin().id;
        for (const Edge& e : g.neighbors(u)) {
            long long nd = r.dist[u] + e.weight;
            if (nd < r.dist[e.to]) {
                r.dist[e.to] = nd;
                r.prev[e.to] = u;
                Key k{nd, 0, e.to};
                if (pq.contains(e.to)) pq.decreaseKey(e.to, k);
                else pq.insert(e.to, k);
            }
        }
    }
    r.heapOps = pq.counters();
    return r;
}

std::vector<int> reconstructPath(const DijkstraResult& r, int dest) {
    if (dest < 1 || dest >= static_cast<int>(r.dist.size()) || r.dist[dest] >= INF_DIST) return {};
    std::vector<int> path;
    for (int v = dest; v != -1; v = r.prev[v]) path.push_back(v);
    std::reverse(path.begin(), path.end());
    return path;
}

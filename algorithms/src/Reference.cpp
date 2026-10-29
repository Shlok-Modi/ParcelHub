#include "Reference.h"
#include <algorithm>
#include <functional>
#include <numeric>
#include <queue>
#include <sstream>

namespace {
struct Dsu {
    std::vector<int> parent;
    explicit Dsu(int n) : parent(n + 1) { std::iota(parent.begin(), parent.end(), 0); }
    int find(int x) {
        while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
        return x;
    }
    bool unite(int a, int b) {
        a = find(a); b = find(b);
        if (a == b) return false;
        parent[a] = b;
        return true;
    }
};
}  // namespace

std::vector<long long> referenceDistances(const Graph& g, int source) {
    std::vector<long long> dist(g.nodeCount() + 1, INF_DIST);
    using Item = std::pair<long long, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> q;
    dist[source] = 0;
    q.push({0, source});
    while (!q.empty()) {
        auto [d, u] = q.top();
        q.pop();
        if (d > dist[u]) continue;   // stale entry (lazy deletion)
        for (const Edge& e : g.neighbors(u)) {
            long long nd = d + e.weight;
            if (nd < dist[e.to]) { dist[e.to] = nd; q.push({nd, e.to}); }
        }
    }
    return dist;
}

ReferenceMst referenceMst(const Graph& g) {
    struct E { int u, v; long long w; };
    std::vector<E> edges;
    for (int u = 1; u <= g.nodeCount(); ++u)
        for (const Edge& e : g.neighbors(u))
            if (u < e.to) edges.push_back({u, e.to, e.weight});
    std::sort(edges.begin(), edges.end(), [](const E& a, const E& b) { return a.w < b.w; });
    Dsu dsu(g.nodeCount());
    ReferenceMst r;
    r.components = g.nodeCount();
    for (const E& e : edges)
        if (dsu.unite(e.u, e.v)) { r.totalWeight += e.w; --r.components; }
    return r;
}

std::string verifyDijkstra(const Graph& g, const DijkstraResult& r, const std::vector<long long>& refDist) {
    std::ostringstream err;
    const std::size_t n = static_cast<std::size_t>(g.nodeCount()) + 1;
    if (r.dist.size() != n || r.prev.size() != n || refDist.size() != n) return "Dijkstra result has the wrong size";
    for (int v = 1; v <= g.nodeCount(); ++v) {
        if (r.dist[v] != refDist[v]) {
            err << "distance to H" << v << " is " << r.dist[v] << ", reference says " << refDist[v];
            return err.str();
        }
        if (v == r.source || r.dist[v] >= INF_DIST) {
            if (r.prev[v] != -1) { err << "H" << v << " should have no predecessor"; return err.str(); }
            continue;
        }
        int p = r.prev[v];
        if (!g.isValidNode(p) || r.dist[p] >= INF_DIST) { err << "H" << v << " has an invalid predecessor"; return err.str(); }
        bool tight = false;   // some road p -> v must explain the distance exactly
        for (const Edge& e : g.neighbors(p))
            if (e.to == v && r.dist[p] + e.weight == r.dist[v]) { tight = true; break; }
        if (!tight) { err << "predecessor of H" << v << " does not explain its distance"; return err.str(); }
    }
    return "";
}

std::string verifyPrim(const Graph& g, const PrimResult& r, const ReferenceMst& ref) {
    std::ostringstream err;
    if (r.totalWeight != ref.totalWeight) {
        err << "total weight " << r.totalWeight << ", reference (Kruskal) says " << ref.totalWeight;
        return err.str();
    }
    if (r.components != ref.components) {
        err << "reports " << r.components << " components, reference says " << ref.components;
        return err.str();
    }
    if (static_cast<long long>(r.edges.size()) != static_cast<long long>(g.nodeCount()) - ref.components) {
        err << "has " << r.edges.size() << " edges, a spanning forest needs " << (g.nodeCount() - ref.components);
        return err.str();
    }
    if (r.isSpanningTree != (ref.components == 1)) return "isSpanningTree flag is wrong";
    Dsu dsu(g.nodeCount());
    long long sum = 0;
    for (const MstEdge& e : r.edges) {
        if (!g.isValidNode(e.u) || !g.isValidNode(e.v)) return "edge uses a hub that does not exist";
        bool real = false;
        for (const Edge& ed : g.neighbors(e.u))
            if (ed.to == e.v && ed.weight == e.weight) { real = true; break; }
        if (!real) { err << "edge H" << e.u << "-H" << e.v << " (" << e.weight << ") is not a road in the graph"; return err.str(); }
        if (!dsu.unite(e.u, e.v)) { err << "edge H" << e.u << "-H" << e.v << " closes a cycle"; return err.str(); }
        sum += e.weight;
    }
    if (sum != r.totalWeight) return "edge weights do not add up to the reported total";
    return "";
}

std::string verifyOrder(const std::vector<int>& got, const std::vector<int>& expected) {
    if (got.size() != expected.size()) {
        std::ostringstream err;
        err << "extracted " << got.size() << " items, expected " << expected.size();
        return err.str();
    }
    for (std::size_t i = 0; i < got.size(); ++i)
        if (got[i] != expected[i]) {
            std::ostringstream err;
            err << "order differs at position " << i << ": got id " << got[i] << ", expected id " << expected[i];
            return err.str();
        }
    return "";
}

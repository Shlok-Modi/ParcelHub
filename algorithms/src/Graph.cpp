#include "Graph.h"
#include <stdexcept>

Graph::Graph(int n) : n_(n), adj_(n > 0 ? n + 1 : 1) {
    if (n <= 0) throw std::invalid_argument("Graph: node count must be positive");
}

void Graph::addEdge(int u, int v, long long w) {
    if (!isValidNode(u) || !isValidNode(v)) throw std::invalid_argument("Graph::addEdge: hub out of range");
    if (u == v) throw std::invalid_argument("Graph::addEdge: self-loops are not allowed");
    if (w < 0) throw std::invalid_argument("Graph::addEdge: negative weight (Dijkstra requires non-negative)");
    adj_[u].push_back({v, w});
    adj_[v].push_back({u, w});
    ++edges_;
}

const std::vector<Edge>& Graph::neighbors(int u) const {
    if (!isValidNode(u)) throw std::invalid_argument("Graph::neighbors: hub out of range");
    return adj_[u];
}

bool Graph::isConnected() const {
    std::vector<char> seen(n_ + 1, 0);
    std::vector<int> stack{1};
    seen[1] = 1;
    int count = 0;
    while (!stack.empty()) {
        int u = stack.back();
        stack.pop_back();
        ++count;
        for (const Edge& e : adj_[u])
            if (!seen[e.to]) { seen[e.to] = 1; stack.push_back(e.to); }
    }
    return count == n_;
}

Graph makeSampleGraph() {
    Graph g(8);
    g.addEdge(1, 2, 7);
    g.addEdge(1, 3, 9);
    g.addEdge(1, 6, 14);
    g.addEdge(2, 3, 10);
    g.addEdge(2, 4, 15);
    g.addEdge(3, 4, 11);
    g.addEdge(3, 6, 2);
    g.addEdge(4, 5, 6);
    g.addEdge(5, 6, 9);
    g.addEdge(5, 7, 3);
    return g;  // H8 has no edges
}

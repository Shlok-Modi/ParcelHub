#pragma once
#include <string>
#include <vector>

struct Edge {
    int to;
    long long weight;
};

// Undirected weighted graph. Hubs are numbered 1..n (H1..Hn in output).
class Graph {
public:
    explicit Graph(int nodeCount);                       // throws if nodeCount <= 0
    int nodeCount() const { return n_; }
    std::size_t edgeCount() const { return edges_; }
    bool isValidNode(int v) const { return v >= 1 && v <= n_; }

    // Throws invalid_argument for invalid hubs, self-loops, or negative weights.
    void addEdge(int u, int v, long long weight);
    const std::vector<Edge>& neighbors(int u) const;
    bool isConnected() const;

    static std::string label(int v) { return "H" + std::to_string(v); }

private:
    int n_;
    std::size_t edges_ = 0;
    std::vector<std::vector<Edge>> adj_;
};

// Small hand-checkable graph: 8 hubs, H8 is deliberately isolated.
Graph makeSampleGraph();

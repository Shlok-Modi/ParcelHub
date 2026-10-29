#pragma once
#include <vector>
#include "Graph.h"
#include "PriorityQueue.h"

struct MstEdge {
    int u, v;             // u is the tree vertex the edge came from, v the vertex it added
    long long weight;
    bool operator==(const MstEdge& o) const { return u == o.u && v == o.v && weight == o.weight; }
};

struct PrimResult {
    std::vector<MstEdge> edges;        // in the order Prim added them
    long long totalWeight = 0;
    int components = 0;                // connected components of the graph
    bool isSpanningTree = false;       // true only if the graph is connected (components == 1)
    std::vector<int> componentOf;      // componentOf[hub] in 1..components (index 0 unused)
    OpCounters heapOps;
};

// Prim's algorithm using decrease-key on any PriorityQueue (which must be empty).
// Grows a tree from `start`. If the graph is disconnected it then continues from
// the lowest-numbered unvisited hub of each remaining component, so the result is
// a minimum spanning FOREST; isSpanningTree is false and `components` says how
// many trees there are. Callers must not present such a result as a full MST.
// Handles zero-weight edges and parallel edges (the cheapest one wins).
PrimResult prim(const Graph& g, int start, PriorityQueue& pq);

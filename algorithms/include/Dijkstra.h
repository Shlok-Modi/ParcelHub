#pragma once
#include <climits>
#include <vector>
#include "Graph.h"
#include "PriorityQueue.h"

constexpr long long INF_DIST = LLONG_MAX / 4;

struct DijkstraResult {
    int source = 0;
    std::vector<long long> dist;  // INF_DIST if unreachable; index = hub number
    std::vector<int> prev;        // predecessor hub, -1 if none
    OpCounters heapOps;
};

// `pq` must be empty; any PriorityQueue implementation can be passed in.
// Uses decrease-key (not lazy deletion) so heaps are compared like for like.
DijkstraResult dijkstra(const Graph& g, int source, PriorityQueue& pq);

// Returns hubs from source to dest inclusive, or empty if dest is unreachable.
std::vector<int> reconstructPath(const DijkstraResult& r, int dest);

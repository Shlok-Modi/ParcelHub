#pragma once
#include <string>
#include <vector>
#include "Dijkstra.h"
#include "Graph.h"
#include "Prim.h"

// Independent reference solutions and result checkers. They use only the standard
// library (std::priority_queue, sorting, union-find), never the project's heaps,
// so a bug in a heap cannot hide itself.

// Shortest distances from `source` (INF_DIST if unreachable); index = hub number.
std::vector<long long> referenceDistances(const Graph& g, int source);

struct ReferenceMst {
    long long totalWeight = 0;   // weight of a minimum spanning forest (Kruskal)
    int components = 0;
};
ReferenceMst referenceMst(const Graph& g);

// Each verifier returns an empty string if the result is correct, otherwise a
// short description of the first problem found.
std::string verifyDijkstra(const Graph& g, const DijkstraResult& r, const std::vector<long long>& refDist);
std::string verifyPrim(const Graph& g, const PrimResult& r, const ReferenceMst& ref);
std::string verifyOrder(const std::vector<int>& got, const std::vector<int>& expected);

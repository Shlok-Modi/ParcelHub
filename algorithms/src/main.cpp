#include <iostream>
#include <memory>
#include <string>
#include "BinaryHeap.h"
#include "Dijkstra.h"
#include "DispatchEngine.h"
#include "Graph.h"

static std::string pathString(const std::vector<int>& path) {
    std::string s;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i) s += " -> ";
        s += Graph::label(path[i]);
    }
    return s;
}

int main() {
    Graph g = makeSampleGraph();
    std::cout << "=== ParcelHub - Phase 1 prototype ===\n";
    std::cout << "City graph: " << g.nodeCount() << " hubs, " << g.edgeCount()
              << " roads, connected: " << (g.isConnected() ? "yes" : "no (H8 is isolated)") << "\n\n";

    // --- Dijkstra from H1 ---
    BinaryHeap heap;
    DijkstraResult dr = dijkstra(g, 1, heap);
    std::cout << "Shortest travel cost from H1:\n";
    for (int v = 1; v <= g.nodeCount(); ++v) {
        std::cout << "  " << Graph::label(v) << ": ";
        if (dr.dist[v] >= INF_DIST) std::cout << "unreachable\n";
        else std::cout << dr.dist[v] << "   (" << pathString(reconstructPath(dr, v)) << ")\n";
    }
    std::cout << "  [" << heap.name() << " ops: " << dr.heapOps.inserts << " inserts, "
              << dr.heapOps.extracts << " extracts, " << dr.heapOps.decreaseKeys
              << " decrease-keys, " << dr.heapOps.comparisons << " comparisons]\n\n";

    // --- Dispatch simulation ---
    DispatchEngine engine(g, std::make_unique<BinaryHeap>());
    engine.addParcel(makeParcel(101, 1, 5, ServiceLevel::Standard, 17 * 60, 0));
    engine.addParcel(makeParcel(102, 1, 7, ServiceLevel::Express,  14 * 60 + 30, 1));
    engine.addParcel(makeParcel(103, 2, 4, ServiceLevel::SameDay,  16 * 60, 2));
    engine.addParcel(makeParcel(104, 3, 5, ServiceLevel::Express,  13 * 60, 3));
    engine.addParcel(makeParcel(105, 1, 8, ServiceLevel::Standard, 18 * 60, 4));
    engine.addParcel(makeParcel(106, 6, 4, ServiceLevel::Standard, 18 * 60, 5));
    std::cout << "Added " << engine.totalCount() << " parcels (" << engine.pendingCount() << " pending).\n";

    engine.upgradePriority(106, ServiceLevel::Express);
    std::cout << "Upgraded P106 from Standard to Express.\n\n";

    int step = 1;
    while (auto r = engine.dispatchNext()) {
        const Parcel& p = r->parcel;
        std::cout << "Dispatch #" << step++ << ": P" << p.id << "  [" << toString(p.level)
                  << ", arrival " << p.arrival << ", deadline " << formatTime(p.deadlineMinutes) << "]\n";
        std::cout << "  Route : " << Graph::label(p.source) << " to " << Graph::label(p.destination) << "\n";
        if (r->reachable) {
            std::cout << "  Path  : " << pathString(r->path) << "\n";
            std::cout << "  Cost  : " << r->cost << "\n";
        } else {
            std::cout << "  Path  : none - destination unreachable\n";
        }
        std::cout << "  Pending: " << engine.pendingCount() << "  Dispatched: " << engine.dispatchedCount() << "\n\n";
    }
    const OpCounters& c = engine.queue().counters();
    std::cout << "Dispatch queue (" << engine.queue().name() << ") ops: " << c.inserts << " inserts, "
              << c.extracts << " extracts, " << c.decreaseKeys << " decrease-keys, "
              << c.comparisons << " comparisons\n";
    return 0;
}

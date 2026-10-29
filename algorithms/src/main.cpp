#include <iostream>
#include <iomanip>
#include <memory>
#include <string>
#include <vector>
#include "Dijkstra.h"
#include "DispatchEngine.h"
#include "Graph.h"
#include "HeapFactory.h"

static std::string pathString(const std::vector<int>& path) {
    std::string s;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i) s += " -> ";
        s += Graph::label(path[i]);
    }
    return s;
}

struct ScenarioResult {
    std::vector<int> order;
    OpCounters ops;
};

// Same six parcels, one upgrade, dispatch everything. Prints details if verbose.
static ScenarioResult runScenario(const Graph& g, HeapType type, bool verbose) {
    DispatchEngine engine(g, type);
    engine.addParcel(makeParcel(101, 1, 5, ServiceLevel::Standard, 17 * 60, 0));
    engine.addParcel(makeParcel(102, 1, 7, ServiceLevel::Express,  14 * 60 + 30, 1));
    engine.addParcel(makeParcel(103, 2, 4, ServiceLevel::SameDay,  16 * 60, 2));
    engine.addParcel(makeParcel(104, 3, 5, ServiceLevel::Express,  13 * 60, 3));
    engine.addParcel(makeParcel(105, 1, 8, ServiceLevel::Standard, 18 * 60, 4));
    engine.addParcel(makeParcel(106, 6, 4, ServiceLevel::Standard, 18 * 60, 5));
    if (verbose) std::cout << "Added " << engine.totalCount() << " parcels. ";
    engine.upgradePriority(106, ServiceLevel::Express);
    if (verbose) std::cout << "Upgraded P106 from Standard to Express.\n\n";

    ScenarioResult out;
    int step = 1;
    while (auto r = engine.dispatchNext()) {
        const Parcel& p = r->parcel;
        out.order.push_back(p.id);
        if (!verbose) continue;
        std::cout << "Dispatch #" << step++ << ": P" << p.id << "  [" << toString(p.level)
                  << ", arrival " << p.arrival << ", deadline " << formatTime(p.deadlineMinutes) << "]\n"
                  << "  Route : " << Graph::label(p.source) << " to " << Graph::label(p.destination) << "\n";
        if (r->reachable)
            std::cout << "  Path  : " << pathString(r->path) << "\n  Cost  : " << r->cost << "\n";
        else
            std::cout << "  Path  : none - destination unreachable\n";
        std::cout << "  Pending: " << engine.pendingCount() << "  Dispatched: " << engine.dispatchedCount() << "\n\n";
    }
    out.ops = engine.queue().counters();
    return out;
}

static void mergeDemo(const Graph& g, HeapType type) {
    DispatchEngine hubA(g, type), hubB(g, type);
    hubA.addParcel(makeParcel(201, 1, 4, ServiceLevel::Express,  0, 0));
    hubA.addParcel(makeParcel(202, 2, 7, ServiceLevel::Standard, 0, 1));
    hubB.addParcel(makeParcel(203, 3, 5, ServiceLevel::SameDay,  0, 2));
    hubB.addParcel(makeParcel(204, 6, 1, ServiceLevel::Express,  0, 3));
    std::cout << "Hub A has " << hubA.pendingCount() << " parcels, Hub B has " << hubB.pendingCount() << ".\n";
    hubA.mergeFrom(hubB);
    std::cout << "After merge: Hub A has " << hubA.pendingCount() << ", Hub B has " << hubB.pendingCount()
              << ". Dispatch order:";
    while (auto r = hubA.dispatchNext()) std::cout << " P" << r->parcel.id;
    std::cout << "\n\n";
}

int main(int argc, char** argv) {
    HeapType type = HeapType::Binary;
    if (argc > 1) {
        auto parsed = parseHeapType(argv[1]);
        if (!parsed) {
            std::cerr << "Usage: parcelhub [binary|binomial|fibonacci]\n";
            return 1;
        }
        type = *parsed;
    }

    Graph g = makeSampleGraph();
    std::cout << "=== ParcelHub - Phase 2 (using " << toString(type) << ") ===\n";
    std::cout << "City graph: " << g.nodeCount() << " hubs, " << g.edgeCount()
              << " roads, connected: " << (g.isConnected() ? "yes" : "no (H8 is isolated)") << "\n\n";

    auto heap = makeHeap(type);
    DijkstraResult dr = dijkstra(g, 1, *heap);
    std::cout << "Shortest travel cost from H1:\n";
    for (int v = 1; v <= g.nodeCount(); ++v) {
        std::cout << "  " << Graph::label(v) << ": ";
        if (dr.dist[v] >= INF_DIST) std::cout << "unreachable\n";
        else std::cout << dr.dist[v] << "   (" << pathString(reconstructPath(dr, v)) << ")\n";
    }
    std::cout << "\n";

    runScenario(g, type, true);

    std::cout << "--- Merging two hub queues ---\n";
    mergeDemo(g, type);

    std::cout << "--- Same workload on every heap ---\n";
    std::cout << std::left << std::setw(16) << "Heap" << std::right << std::setw(9) << "inserts"
              << std::setw(9) << "extracts" << std::setw(13) << "decrease-key"
              << std::setw(13) << "comparisons" << "   dispatch order\n";
    std::vector<int> reference;
    bool identical = true;
    for (HeapType t : kAllHeapTypes) {
        ScenarioResult r = runScenario(g, t, false);
        if (reference.empty()) reference = r.order;
        identical = identical && (r.order == reference);
        std::cout << std::left << std::setw(16) << toString(t) << std::right << std::setw(9) << r.ops.inserts
                  << std::setw(9) << r.ops.extracts << std::setw(13) << r.ops.decreaseKeys
                  << std::setw(13) << r.ops.comparisons << "   ";
        for (int id : r.order) std::cout << "P" << id << " ";
        std::cout << "\n";
    }
    std::cout << "Dispatch order identical across all heaps: " << (identical ? "yes" : "NO") << "\n";
    return identical ? 0 : 2;
}

// parcelhub_api: the executable the Express backend runs.
//
// Reads ONE request from standard input and writes ONE JSON object to standard output.
// The request is a sequence of whitespace-separated tokens (no JSON parser needed, so
// there is nothing for a malicious payload to confuse). Every number is range-checked
// and every size is capped here as well as in the backend: this program is safe to run
// on untrusted input even if the backend's checks were bypassed.
//
// Request grammar (<...> is one token):
//   route    <heap> <n> <m> {<u> <v> <w>}*m <source> <dest|0>
//   mst      <heap> <n> <m> {<u> <v> <w>}*m <start>
//   dispatch <heap> <n> <m> {<u> <v> <w>}*m <k> {<id> <src> <dst> <level 1-3> <deadlineMin> <arrival>}*k
//                                           <j> {<id> <newLevel 1-3>}*j
//   merge    <heap> <n> <m> {<u> <v> <w>}*m <ka> {<id> <src> <dst> <level 1-3> <deadlineMin> <arrival>}*ka
//                                           <kb> {<id> <src> <dst> <level 1-3> <deadlineMin> <arrival>}*kb
//   bench    <seed> <repeats> <warmups> <instances>
//            <ng> {<hubs>}*ng  <np> {<parcels>}*np  <nd> {<density>}*nd  <nb> {<benchmark>}*nb
//   heap = binary | binomial | fibonacci
//
// Exit codes: 0 success, 2 invalid request (JSON error on stdout), 3 internal error.
#include <algorithm>
#include <iostream>
#include <iterator>
#include <sstream>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>
#include "Benchmark.h"
#include "BenchmarkExport.h"
#include "Dijkstra.h"
#include "DispatchEngine.h"
#include "Graph.h"
#include "HeapFactory.h"
#include "Prim.h"

namespace {

constexpr long long kMaxHubs = 5000, kMaxRoads = 100000, kMaxWeight = 1000000000LL;
constexpr long long kMaxParcels = 20000, kMaxUpgrades = 20000, kMaxWork = 50000000LL;
constexpr long long kMaxBenchHubs = 3000, kMaxBenchParcels = 50000;
constexpr std::size_t kMaxListLength = 8;

struct RequestError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Tokens {
public:
    explicit Tokens(std::istream& in) : in_(in) {}
    std::string word(const char* what) {
        std::string t;
        if (!(in_ >> t)) throw RequestError(std::string("missing value: ") + what);
        if (t.size() > 64) throw RequestError(std::string("value too long: ") + what);
        return t;
    }
    long long integer(const char* what, long long lo, long long hi) {
        std::string t = word(what);
        std::size_t used = 0;
        long long v = 0;
        try { v = std::stoll(t, &used); }
        catch (const std::exception&) { used = 0; }
        if (used == 0 || used != t.size() || v < lo || v > hi)
            throw RequestError(std::string(what) + " must be a whole number from " + std::to_string(lo) + " to " + std::to_string(hi));
        return v;
    }
    void expectEnd() {
        std::string t;
        if (in_ >> t) throw RequestError("unexpected extra input after the end of the request");
    }
private:
    std::istream& in_;
};

HeapType readHeap(Tokens& t) {
    auto h = parseHeapType(t.word("heap"));
    if (!h) throw RequestError("heap must be binary, binomial or fibonacci");
    return *h;
}

Graph readGraph(Tokens& t) {
    int n = static_cast<int>(t.integer("hub count", 1, kMaxHubs));
    long long m = t.integer("road count", 0, kMaxRoads);
    Graph g(n);
    for (long long i = 0; i < m; ++i) {
        int u = static_cast<int>(t.integer("road endpoint", 1, n));
        int v = static_cast<int>(t.integer("road endpoint", 1, n));
        long long w = t.integer("road weight", 0, kMaxWeight);
        if (u == v) throw RequestError("a road cannot connect a hub to itself");
        g.addEdge(u, v, w);
    }
    return g;
}

std::string heapOpsJson(const OpCounters& c) {
    std::ostringstream s;
    s << "{\"inserts\":" << c.inserts << ",\"extracts\":" << c.extracts << ",\"decreaseKeys\":" << c.decreaseKeys
      << ",\"merges\":" << c.merges << ",\"comparisons\":" << c.comparisons << "}";
    return s.str();
}

std::string pathJson(const std::vector<int>& path) {
    std::ostringstream s;
    s << "[";
    for (std::size_t i = 0; i < path.size(); ++i) s << (i ? "," : "") << path[i];
    s << "]";
    return s.str();
}

std::string runRoute(Tokens& t) {
    HeapType type = readHeap(t);
    Graph g = readGraph(t);
    int source = static_cast<int>(t.integer("source hub", 1, g.nodeCount()));
    int dest = static_cast<int>(t.integer("destination hub (0 for none)", 0, g.nodeCount()));
    t.expectEnd();

    auto pq = makeHeap(type);
    DijkstraResult r = dijkstra(g, source, *pq);
    std::ostringstream s;
    s << "{\"ok\":true,\"command\":\"route\",\"heap\":\"" << jsonEscape(toString(type)) << "\",\"source\":" << source << ",\"distances\":[";
    for (int v = 1; v <= g.nodeCount(); ++v) {
        if (v > 1) s << ",";
        if (r.dist[v] >= INF_DIST) s << "null"; else s << r.dist[v];
    }
    s << "]";
    if (dest != 0) {
        std::vector<int> path = reconstructPath(r, dest);
        s << ",\"destination\":" << dest << ",\"reachable\":" << (path.empty() ? "false" : "true") << ",\"path\":" << pathJson(path);
        if (!path.empty()) s << ",\"cost\":" << r.dist[dest]; else s << ",\"cost\":null";
    }
    s << ",\"heapOps\":" << heapOpsJson(r.heapOps) << "}";
    return s.str();
}

std::string runMst(Tokens& t) {
    HeapType type = readHeap(t);
    Graph g = readGraph(t);
    int start = static_cast<int>(t.integer("start hub", 1, g.nodeCount()));
    t.expectEnd();

    auto pq = makeHeap(type);
    PrimResult r = prim(g, start, *pq);
    std::ostringstream s;
    s << "{\"ok\":true,\"command\":\"mst\",\"heap\":\"" << jsonEscape(toString(type)) << "\",\"start\":" << start
      << ",\"totalWeight\":" << r.totalWeight << ",\"components\":" << r.components
      << ",\"isSpanningTree\":" << (r.isSpanningTree ? "true" : "false") << ",\"edges\":[";
    for (std::size_t i = 0; i < r.edges.size(); ++i)
        s << (i ? "," : "") << "{\"from\":" << r.edges[i].u << ",\"to\":" << r.edges[i].v << ",\"weight\":" << r.edges[i].weight << "}";
    s << "],\"heapOps\":" << heapOpsJson(r.heapOps) << "}";
    return s.str();
}

std::vector<Parcel> readParcels(Tokens& t, const Graph& g, long long k) {
    std::vector<Parcel> parcels;
    for (long long i = 0; i < k; ++i) {
        int id = static_cast<int>(t.integer("parcel id", 1, 1000000000));
        int src = static_cast<int>(t.integer("parcel source", 1, g.nodeCount()));
        int dst = static_cast<int>(t.integer("parcel destination", 1, g.nodeCount()));
        int level = static_cast<int>(t.integer("service level", 1, 3));
        int deadline = static_cast<int>(t.integer("deadline minutes", 0, 1439));
        int arrival = static_cast<int>(t.integer("arrival", 0, 1000000000));
        parcels.push_back(makeParcel(id, src, dst, static_cast<ServiceLevel>(level), deadline, arrival));
    }
    return parcels;
}

// One dispatched parcel as a JSON object WITHOUT the closing brace, so callers can add fields.
std::string dispatchEntryOpen(const DispatchResult& r) {
    const Parcel& p = r.parcel;
    std::ostringstream s;
    s << "{\"id\":" << p.id << ",\"level\":\"" << jsonEscape(toString(p.level)) << "\",\"priority\":" << p.priority
      << ",\"arrival\":" << p.arrival << ",\"deadline\":\"" << formatTime(p.deadlineMinutes) << "\",\"source\":" << p.source
      << ",\"destination\":" << p.destination << ",\"reachable\":" << (r.reachable ? "true" : "false")
      << ",\"path\":" << pathJson(r.path) << ",\"cost\":";
    if (r.reachable) s << r.cost; else s << "null";
    return s.str();
}

std::string runDispatch(Tokens& t) {
    HeapType type = readHeap(t);
    Graph g = readGraph(t);
    long long k = t.integer("parcel count", 0, kMaxParcels);
    if (k * (static_cast<long long>(g.nodeCount()) + static_cast<long long>(g.edgeCount()) + 1) > kMaxWork)
        throw RequestError("workload too large: reduce the number of parcels or the size of the city graph");
    DispatchEngine engine(g, type);
    std::vector<Parcel> parcels = readParcels(t, g, k);
    long long j = t.integer("upgrade count", 0, kMaxUpgrades);
    std::vector<std::pair<int, ServiceLevel>> upgrades;
    for (long long i = 0; i < j; ++i) {
        int id = static_cast<int>(t.integer("upgrade parcel id", 1, 1000000000));
        int level = static_cast<int>(t.integer("upgrade level", 1, 3));
        upgrades.push_back({id, static_cast<ServiceLevel>(level)});
    }
    t.expectEnd();

    try {
        for (const Parcel& p : parcels) engine.addParcel(p);
        for (const auto& u : upgrades) engine.upgradePriority(u.first, u.second);
    } catch (const std::invalid_argument& e) {
        throw RequestError(e.what());
    }

    std::ostringstream s;
    s << "{\"ok\":true,\"command\":\"dispatch\",\"heap\":\"" << jsonEscape(toString(type)) << "\",\"dispatches\":[";
    bool first = true;
    while (auto r = engine.dispatchNext()) {
        s << (first ? "" : ",") << dispatchEntryOpen(*r) << "}";
        first = false;
    }
    s << "],\"heapOps\":" << heapOpsJson(engine.queue().counters()) << "}";
    return s.str();
}

std::string runMerge(Tokens& t) {
    HeapType type = readHeap(t);
    Graph g = readGraph(t);
    long long ka = t.integer("hub A parcel count", 0, kMaxParcels);
    std::vector<Parcel> a = readParcels(t, g, ka);
    long long kb = t.integer("hub B parcel count", 0, kMaxParcels);
    if (ka + kb > kMaxParcels) throw RequestError("too many parcels in total");
    if ((ka + kb) * (static_cast<long long>(g.nodeCount()) + static_cast<long long>(g.edgeCount()) + 1) > kMaxWork)
        throw RequestError("workload too large: reduce the number of parcels or the size of the city graph");
    std::vector<Parcel> b = readParcels(t, g, kb);
    t.expectEnd();

    DispatchEngine hubA(g, type), hubB(g, type);
    try {
        for (const Parcel& p : a) hubA.addParcel(p);
        for (const Parcel& p : b) hubB.addParcel(p);
        hubA.mergeFrom(hubB);
    } catch (const std::invalid_argument& e) {
        throw RequestError(e.what());
    }

    // What the merged queue must contain and in which order, worked out without using any heap.
    std::unordered_map<int, char> origin;
    std::vector<std::tuple<int, int, int>> expected;   // (priority, arrival, id): the dispatch key
    for (const Parcel& p : a) { origin[p.id] = 'A'; expected.emplace_back(p.priority, p.arrival, p.id); }
    for (const Parcel& p : b) { origin[p.id] = 'B'; expected.emplace_back(p.priority, p.arrival, p.id); }
    std::sort(expected.begin(), expected.end());

    const std::size_t total = a.size() + b.size();
    const bool countsMatch = hubA.pendingCount() == total && hubA.totalCount() == total;
    const bool hubBEmpty = hubB.pendingCount() == 0 && hubB.totalCount() == 0;

    std::ostringstream entries;
    std::vector<int> order;
    std::set<int> seen;
    bool duplicates = false;
    while (auto r = hubA.dispatchNext()) {
        if (!seen.insert(r->parcel.id).second) duplicates = true;
        order.push_back(r->parcel.id);
        entries << (order.size() > 1 ? "," : "") << dispatchEntryOpen(*r) << ",\"origin\":\"" << origin[r->parcel.id] << "\"}";
    }
    bool noLoss = true;
    for (const auto& e : expected) if (!seen.count(std::get<2>(e))) noLoss = false;
    bool orderMatches = order.size() == expected.size();
    for (std::size_t i = 0; orderMatches && i < order.size(); ++i) orderMatches = order[i] == std::get<2>(expected[i]);
    const bool verified = countsMatch && hubBEmpty && noLoss && !duplicates && orderMatches;

    OpCounters ops = hubA.queue().counters();            // hub B's own work (its inserts) is added in
    const OpCounters& ob = hubB.queue().counters();
    ops.inserts += ob.inserts; ops.extracts += ob.extracts; ops.decreaseKeys += ob.decreaseKeys;
    ops.merges += ob.merges; ops.comparisons += ob.comparisons;

    auto yes = [](bool v) { return v ? "true" : "false"; };
    std::ostringstream s;
    s << "{\"ok\":true,\"command\":\"merge\",\"heap\":\"" << jsonEscape(toString(type)) << "\",\"hubA\":{\"parcels\":" << ka
      << "},\"hubB\":{\"parcels\":" << kb << "},\"merged\":" << total << ",\"verified\":" << yes(verified)
      << ",\"checks\":{\"countsMatch\":" << yes(countsMatch) << ",\"hubBEmpty\":" << yes(hubBEmpty) << ",\"noParcelLost\":" << yes(noLoss)
      << ",\"noDuplicates\":" << yes(!duplicates) << ",\"orderMatchesSortedKeys\":" << yes(orderMatches)
      << "},\"dispatches\":[" << entries.str() << "],\"heapOps\":" << heapOpsJson(ops) << "}";
    return s.str();
}

std::string runBench(Tokens& t) {
    BenchConfig cfg;
    cfg.profile = "api";
    cfg.seed = static_cast<uint64_t>(t.integer("seed", 0, 4000000000LL));
    cfg.repeats = static_cast<int>(t.integer("repeats", 1, 10));
    cfg.warmups = static_cast<int>(t.integer("warmups", 0, 3));
    cfg.instances = static_cast<int>(t.integer("instances", 1, 3));
    cfg.maxDenseNodes = 1000;

    auto count = [&](const char* what) { return static_cast<std::size_t>(t.integer(what, 0, static_cast<long long>(kMaxListLength))); };
    cfg.graphSizes.clear();
    for (std::size_t i = 0, n = count("graph size count"); i < n; ++i) cfg.graphSizes.push_back(static_cast<int>(t.integer("graph size", 2, kMaxBenchHubs)));
    cfg.parcelSizes.clear();
    for (std::size_t i = 0, n = count("parcel size count"); i < n; ++i) cfg.parcelSizes.push_back(static_cast<int>(t.integer("parcel size", 2, kMaxBenchParcels)));
    cfg.densities.clear();
    for (std::size_t i = 0, n = count("density count"); i < n; ++i) {
        auto d = parseDensity(t.word("density"));
        if (!d) throw RequestError("density must be sparse, medium or dense");
        cfg.densities.push_back(*d);
    }
    cfg.runDijkstra = cfg.runPrim = cfg.runDispatch = cfg.runMerge = false;
    std::size_t nb = count("benchmark count");
    for (std::size_t i = 0; i < nb; ++i) {
        std::string b = t.word("benchmark");
        if (b == "dijkstra") cfg.runDijkstra = true;
        else if (b == "prim") cfg.runPrim = true;
        else if (b == "dispatch") cfg.runDispatch = true;
        else if (b == "merge") cfg.runMerge = true;
        else throw RequestError("benchmark must be dijkstra, prim, dispatch or merge");
    }
    t.expectEnd();
    if (!(cfg.runDijkstra || cfg.runPrim || cfg.runDispatch || cfg.runMerge)) throw RequestError("choose at least one benchmark");
    if ((cfg.runDijkstra || cfg.runPrim) && (cfg.graphSizes.empty() || cfg.densities.empty()))
        throw RequestError("graph benchmarks need at least one graph size and one density");
    if ((cfg.runDispatch || cfg.runMerge) && cfg.parcelSizes.empty()) throw RequestError("dispatch and merge benchmarks need at least one parcel size");

    BenchReport report = runBenchmarks(cfg, nullptr);
    return "{\"ok\":true,\"command\":\"bench\",\"report\":" + toJson(report) + "}";
}

}  // namespace

int main() {
    std::ios::sync_with_stdio(false);
    try {
        Tokens t(std::cin);
        std::string command = t.word("command");
        std::string out;
        if (command == "route") out = runRoute(t);
        else if (command == "mst") out = runMst(t);
        else if (command == "dispatch") out = runDispatch(t);
        else if (command == "merge") out = runMerge(t);
        else if (command == "bench") out = runBench(t);
        else throw RequestError("unknown command (use route, mst, dispatch, merge or bench)");
        std::cout << out << std::endl;
        return 0;
    } catch (const RequestError& e) {
        std::cout << "{\"ok\":false,\"error\":\"" << jsonEscape(e.what()) << "\"}" << std::endl;
        return 2;
    } catch (const std::invalid_argument& e) {
        std::cout << "{\"ok\":false,\"error\":\"" << jsonEscape(e.what()) << "\"}" << std::endl;
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "internal error: " << e.what() << "\n";
        std::cout << "{\"ok\":false,\"error\":\"internal error\"}" << std::endl;
        return 3;
    }
}

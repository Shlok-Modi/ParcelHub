#include "DispatchEngine.h"
#include "BinaryHeap.h"
#include "Dijkstra.h"
#include <stdexcept>

DispatchEngine::DispatchEngine(const Graph& graph, std::unique_ptr<PriorityQueue> queue)
    : graph_(graph), queue_(std::move(queue)) {
    if (!queue_) throw std::invalid_argument("DispatchEngine: queue is null");
}

void DispatchEngine::addParcel(const Parcel& p) {
    if (parcels_.count(p.id)) throw std::invalid_argument("addParcel: duplicate parcel id");
    if (!graph_.isValidNode(p.source) || !graph_.isValidNode(p.destination))
        throw std::invalid_argument("addParcel: source/destination hub not in graph");
    Parcel copy = p;
    copy.status = Status::Pending;
    queue_->insert(copy.id, keyFor(copy));
    parcels_[copy.id] = copy;
}

void DispatchEngine::upgradePriority(int id, ServiceLevel newLevel) {
    auto it = parcels_.find(id);
    if (it == parcels_.end()) throw std::invalid_argument("upgradePriority: unknown parcel id");
    Parcel& p = it->second;
    if (p.status == Status::Dispatched) throw std::invalid_argument("upgradePriority: parcel already dispatched");
    int np = static_cast<int>(newLevel);
    if (np >= p.priority) throw std::invalid_argument("upgradePriority: not an upgrade");
    Parcel updated = p;
    updated.level = newLevel;
    updated.priority = np;
    queue_->decreaseKey(id, keyFor(updated));  // may throw; p untouched until it succeeds
    p = updated;
}

const Parcel* DispatchEngine::peekNext() const {
    if (queue_->empty()) return nullptr;
    return &parcels_.at(queue_->peekMin().id);
}

const Parcel* DispatchEngine::find(int id) const {
    auto it = parcels_.find(id);
    return it == parcels_.end() ? nullptr : &it->second;
}

std::optional<DispatchResult> DispatchEngine::dispatchNext() {
    if (queue_->empty()) return std::nullopt;
    int id = queue_->extractMin().id;
    Parcel& p = parcels_.at(id);
    p.status = Status::Dispatched;
    ++dispatched_;

    DispatchResult res;
    res.parcel = p;
    BinaryHeap routingHeap;
    DijkstraResult dr = dijkstra(graph_, p.source, routingHeap);
    res.path = reconstructPath(dr, p.destination);
    res.reachable = !res.path.empty();
    if (res.reachable) res.cost = dr.dist[p.destination];
    return res;
}

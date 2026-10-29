#pragma once
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include "Graph.h"
#include "HeapFactory.h"
#include "Parcel.h"
#include "PriorityQueue.h"

struct DispatchResult {
    Parcel parcel;              // status already set to Dispatched
    bool reachable = false;
    std::vector<int> path;      // hubs source..destination (empty if unreachable)
    long long cost = 0;         // meaningful only if reachable
};

class DispatchEngine {
public:
    // Dispatch queue and the heap used inside Dijkstra routing can be chosen separately.
    DispatchEngine(const Graph& graph, std::unique_ptr<PriorityQueue> queue,
                   HeapType routingHeap = HeapType::Binary);
    // Convenience: use the same heap type for both.
    DispatchEngine(const Graph& graph, HeapType type);

    // Throws invalid_argument on duplicate parcel id or hubs not in the graph.
    void addParcel(const Parcel& p);
    // Only genuine upgrades are allowed (new level more urgent than current).
    void upgradePriority(int parcelId, ServiceLevel newLevel);
    // Extracts the next parcel, marks it Dispatched, routes it with Dijkstra.
    std::optional<DispatchResult> dispatchNext();

    // Combines two hubs' queues. Both engines must share the same graph and heap
    // type, and no parcel id may appear in both. On failure nothing changes.
    // `other` is left empty.
    void mergeFrom(DispatchEngine& other);

    const Parcel* peekNext() const;
    const Parcel* find(int id) const;
    std::size_t pendingCount() const { return queue_->size(); }
    std::size_t dispatchedCount() const { return dispatched_; }
    std::size_t totalCount() const { return parcels_.size(); }
    const PriorityQueue& queue() const { return *queue_; }

private:
    const Graph& graph_;
    std::unique_ptr<PriorityQueue> queue_;
    HeapType routingHeap_;
    std::unordered_map<int, Parcel> parcels_;
    std::size_t dispatched_ = 0;

    static Key keyFor(const Parcel& p) { return {p.priority, p.arrival, p.id}; }
};

#pragma once
#include <cstddef>
#include <tuple>

// Composite key: lower compares as higher priority.
// Dispatch uses (priority, arrivalTime, parcelId) so every heap yields the
// same order. Dijkstra uses (distance, 0, vertexId) for determinism.
struct Key {
    long long primary = 0, secondary = 0, tertiary = 0;
    bool operator<(const Key& o) const {
        return std::tie(primary, secondary, tertiary) < std::tie(o.primary, o.secondary, o.tertiary);
    }
    bool operator==(const Key& o) const {
        return std::tie(primary, secondary, tertiary) == std::tie(o.primary, o.secondary, o.tertiary);
    }
};

struct HeapEntry {
    int id;
    Key key;
};

// Operation counters used later by the benchmark engine.
struct OpCounters {
    long long inserts = 0, extracts = 0, decreaseKeys = 0, merges = 0, comparisons = 0;
};

// Common interface so Binomial/Fibonacci heaps can plug in during Phase 2.
// merge() is intentionally NOT here: merging is only safe between heaps of the
// same concrete type, so each heap exposes its own merge(SameType&).
class PriorityQueue {
public:
    virtual ~PriorityQueue() = default;
    virtual void insert(int id, const Key& key) = 0;      // throws invalid_argument on duplicate id
    virtual HeapEntry extractMin() = 0;                   // throws out_of_range if empty
    virtual const HeapEntry& peekMin() const = 0;         // throws out_of_range if empty
    virtual void decreaseKey(int id, const Key& k) = 0;   // throws if id missing or k is larger than current
    virtual bool contains(int id) const = 0;
    virtual bool empty() const = 0;
    virtual std::size_t size() const = 0;
    virtual const OpCounters& counters() const = 0;
    virtual const char* name() const = 0;
};

#pragma once
#include <unordered_map>
#include <vector>
#include "PriorityQueue.h"

// Array-based min-heap with an id -> index map so decrease-key is O(log n).
class BinaryHeap : public PriorityQueue {
public:
    void insert(int id, const Key& key) override;
    HeapEntry extractMin() override;
    const HeapEntry& peekMin() const override;
    void decreaseKey(int id, const Key& k) override;
    bool contains(int id) const override { return pos_.count(id) > 0; }
    bool empty() const override { return a_.empty(); }
    std::size_t size() const override { return a_.size(); }
    const OpCounters& counters() const override { return c_; }
    const char* name() const override { return "Binary Heap"; }

    // O(n + m): append everything, then heapify. Empties `other`.
    // Throws invalid_argument (and changes nothing) if an id exists in both heaps.
    void merge(BinaryHeap& other);

private:
    std::vector<HeapEntry> a_;
    std::unordered_map<int, std::size_t> pos_;
    OpCounters c_;

    bool less(const Key& x, const Key& y) { ++c_.comparisons; return x < y; }
    void swapAt(std::size_t i, std::size_t j);
    void siftUp(std::size_t i);
    void siftDown(std::size_t i);
};

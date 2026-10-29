#pragma once
#include <unordered_map>
#include "PriorityQueue.h"

// Forest of binomial trees; root list sorted by strictly increasing degree.
//   insert / extract-min / decrease-key : O(log n)
//   merge (structural union)            : O(log n)
// Note: the id -> node lookup table also has to be merged, which costs O(m) for
// the smaller heap's m items. The structural union itself is O(log n).
class BinomialHeap : public PriorityQueue {
public:
    BinomialHeap() = default;
    ~BinomialHeap() override;
    BinomialHeap(const BinomialHeap&) = delete;
    BinomialHeap& operator=(const BinomialHeap&) = delete;

    void insert(int id, const Key& key) override;
    HeapEntry extractMin() override;
    const HeapEntry& peekMin() const override;
    void decreaseKey(int id, const Key& k) override;
    bool contains(int id) const override { return nodes_.count(id) > 0; }
    bool empty() const override { return size_ == 0; }
    std::size_t size() const override { return size_; }
    const OpCounters& counters() const override { return c_; }
    const char* name() const override { return "Binomial Heap"; }

    void merge(BinomialHeap& other);
    void mergeFrom(PriorityQueue& other) override;
    bool checkInvariants() const override;
    std::size_t rootCount() const;  // equals popcount(size) after inserts only

private:
    struct Node {
        HeapEntry entry;
        int degree = 0;
        Node* parent = nullptr;
        Node* child = nullptr;    // leftmost child (highest degree)
        Node* sibling = nullptr;  // next root, or next sibling in a child list
        Node(int id, const Key& k) : entry{id, k} {}
    };

    Node* head_ = nullptr;
    std::size_t size_ = 0;
    std::unordered_map<int, Node*> nodes_;
    OpCounters c_;

    bool less(const Key& a, const Key& b) { ++c_.comparisons; return a < b; }
    void link(Node* child, Node* newParent);
    static Node* mergeRootLists(Node* a, Node* b);
    void unionWith(Node* otherHead);
};

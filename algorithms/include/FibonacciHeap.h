#pragma once
#include <unordered_map>
#include "PriorityQueue.h"

// Fibonacci heap: circular doubly-linked root list, min pointer, marks and
// cascading cuts. Amortised: insert O(1), decrease-key O(1), meld O(1) for the
// structure, extract-min O(log n).
// Note: meld also has to validate ids and merge the id -> node table, which is
// O(m) for the other heap's m items; the structural splice is O(1).
class FibonacciHeap : public PriorityQueue {
public:
    FibonacciHeap() = default;
    ~FibonacciHeap() override;
    FibonacciHeap(const FibonacciHeap&) = delete;
    FibonacciHeap& operator=(const FibonacciHeap&) = delete;

    void insert(int id, const Key& key) override;
    HeapEntry extractMin() override;
    const HeapEntry& peekMin() const override;
    void decreaseKey(int id, const Key& k) override;
    bool contains(int id) const override { return nodes_.count(id) > 0; }
    bool empty() const override { return size_ == 0; }
    std::size_t size() const override { return size_; }
    const OpCounters& counters() const override { return c_; }
    const char* name() const override { return "Fibonacci Heap"; }

    void meld(FibonacciHeap& other);
    void mergeFrom(PriorityQueue& other) override;
    bool checkInvariants() const override;

    long long cuts() const { return cuts_; }                    // all cuts (decrease-key + cascading)
    long long cascadingCuts() const { return cascades_; }       // cuts triggered by a marked parent

private:
    struct Node {
        HeapEntry entry;
        Node* parent = nullptr;
        Node* child = nullptr;   // any one child; children form a circular list
        Node* left = nullptr;
        Node* right = nullptr;
        int degree = 0;
        bool mark = false;
        Node(int id, const Key& k) : entry{id, k} {}
    };

    Node* min_ = nullptr;
    std::size_t size_ = 0;
    std::unordered_map<int, Node*> nodes_;
    OpCounters c_;
    long long cuts_ = 0, cascades_ = 0;

    bool less(const Key& a, const Key& b) { ++c_.comparisons; return a < b; }
    void addToRootList(Node* x);
    void link(Node* child, Node* newParent);
    void consolidate();
    void cut(Node* x, Node* parent);
    void cascadingCut(Node* y);
};

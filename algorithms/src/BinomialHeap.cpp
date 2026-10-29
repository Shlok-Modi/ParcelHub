#include "BinomialHeap.h"
#include <stdexcept>
#include <utility>
#include <vector>

BinomialHeap::~BinomialHeap() {
    for (auto& kv : nodes_) delete kv.second;
}

void BinomialHeap::link(Node* y, Node* z) {  // y becomes the newest child of z
    y->parent = z;
    y->sibling = z->child;
    z->child = y;
    ++z->degree;
}

BinomialHeap::Node* BinomialHeap::mergeRootLists(Node* a, Node* b) {
    Node* head = nullptr;
    Node** tail = &head;
    while (a && b) {
        if (a->degree <= b->degree) { *tail = a; tail = &a->sibling; a = a->sibling; }
        else                        { *tail = b; tail = &b->sibling; b = b->sibling; }
    }
    *tail = a ? a : b;
    return head;
}

void BinomialHeap::unionWith(Node* otherHead) {
    Node* h = mergeRootLists(head_, otherHead);
    head_ = h;
    if (!h) return;
    Node* prev = nullptr;
    Node* x = h;
    Node* next = x->sibling;
    while (next) {
        if (x->degree != next->degree || (next->sibling && next->sibling->degree == x->degree)) {
            prev = x;
            x = next;
        } else if (!less(next->entry.key, x->entry.key)) {   // x stays the root
            x->sibling = next->sibling;
            link(next, x);
        } else {                                              // next becomes the root
            if (prev) prev->sibling = next; else h = next;
            link(x, next);
            x = next;
        }
        next = x->sibling;
    }
    head_ = h;
}

void BinomialHeap::insert(int id, const Key& key) {
    if (contains(id)) throw std::invalid_argument("BinomialHeap::insert: duplicate id");
    Node* n = new Node(id, key);
    nodes_[id] = n;
    ++size_;
    unionWith(n);
    ++c_.inserts;
}

const HeapEntry& BinomialHeap::peekMin() const {
    if (!head_) throw std::out_of_range("BinomialHeap::peekMin: heap is empty");
    const Node* best = head_;
    for (const Node* r = head_->sibling; r; r = r->sibling)
        if (r->entry.key < best->entry.key) best = r;
    return best->entry;
}

HeapEntry BinomialHeap::extractMin() {
    if (!head_) throw std::out_of_range("BinomialHeap::extractMin: heap is empty");
    Node* minNode = head_;
    Node* minPrev = nullptr;
    for (Node *prev = head_, *x = head_->sibling; x; prev = x, x = x->sibling)
        if (less(x->entry.key, minNode->entry.key)) { minNode = x; minPrev = prev; }

    if (minPrev) minPrev->sibling = minNode->sibling; else head_ = minNode->sibling;

    Node* rev = nullptr;  // children come out in decreasing degree; reverse them
    for (Node* c = minNode->child; c;) {
        Node* nx = c->sibling;
        c->sibling = rev;
        c->parent = nullptr;
        rev = c;
        c = nx;
    }
    HeapEntry result = minNode->entry;
    nodes_.erase(result.id);
    delete minNode;
    --size_;
    unionWith(rev);
    ++c_.extracts;
    return result;
}

void BinomialHeap::decreaseKey(int id, const Key& k) {
    auto it = nodes_.find(id);
    if (it == nodes_.end()) throw std::invalid_argument("BinomialHeap::decreaseKey: unknown id");
    Node* x = it->second;
    if (x->entry.key < k) throw std::invalid_argument("BinomialHeap::decreaseKey: new key is larger than current key");
    x->entry.key = k;
    while (x->parent && less(x->entry.key, x->parent->entry.key)) {
        std::swap(x->entry, x->parent->entry);       // swap payloads, keep tree shape
        nodes_[x->entry.id] = x;
        nodes_[x->parent->entry.id] = x->parent;
        x = x->parent;
    }
    ++c_.decreaseKeys;
}

void BinomialHeap::merge(BinomialHeap& other) {
    if (&other == this) throw std::invalid_argument("BinomialHeap::merge: cannot merge a heap with itself");
    for (const auto& kv : other.nodes_)
        if (contains(kv.first)) throw std::invalid_argument("BinomialHeap::merge: duplicate id across heaps");
    Node* oh = other.head_;
    other.head_ = nullptr;
    nodes_.insert(other.nodes_.begin(), other.nodes_.end());
    size_ += other.size_;
    other.nodes_.clear();
    other.size_ = 0;
    unionWith(oh);
    ++c_.merges;
}

void BinomialHeap::mergeFrom(PriorityQueue& other) {
    BinomialHeap* o = dynamic_cast<BinomialHeap*>(&other);
    if (!o) throw std::invalid_argument("BinomialHeap::mergeFrom: other heap is a different type");
    merge(*o);
}

std::size_t BinomialHeap::rootCount() const {
    std::size_t n = 0;
    for (const Node* r = head_; r; r = r->sibling) ++n;
    return n;
}

bool BinomialHeap::checkInvariants() const {
    std::size_t count = 0;
    int lastDegree = -1;
    for (const Node* r = head_; r; r = r->sibling) {
        if (r->parent) return false;
        if (r->degree <= lastDegree) return false;   // strictly increasing root degrees
        lastDegree = r->degree;
        std::vector<const Node*> stack{r};
        while (!stack.empty()) {
            const Node* n = stack.back();
            stack.pop_back();
            ++count;
            auto it = nodes_.find(n->entry.id);
            if (it == nodes_.end() || it->second != n) return false;
            int expect = n->degree - 1, kids = 0;
            for (const Node* c = n->child; c; c = c->sibling) {
                if (c->parent != n) return false;
                if (c->degree != expect) return false;           // children are B(k-1) ... B0
                if (c->entry.key < n->entry.key) return false;  // heap order
                --expect; ++kids;
                stack.push_back(c);
            }
            if (kids != n->degree) return false;
        }
    }
    return count == size_ && nodes_.size() == size_;
}
